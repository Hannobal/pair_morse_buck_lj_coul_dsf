/***************************************************************************
                               morse_lj_dsf.cpp
                             -------------------
                            W. Michael Brown (ORNL)

  Class for acceleration of the lj/cut/coul/dsf pair style.

 __________________________________________________________________________
    This file is part of the LAMMPS Accelerator Library (LAMMPS_AL)
 __________________________________________________________________________

    begin                : 7/12/2012
    email                : brownw@ornl.gov
 ***************************************************************************/

#if defined(USE_OPENCL)
#include "morse_buck_lj_dsf_cl.h"
#elif defined(USE_CUDART)
const char *morse_buck_lj_dsf=0;
#else
#include "morse_buck_lj_dsf_cubin.h"
#endif

#include "lal_morse_buck_lj_dsf.h"
#include <cassert>
using namespace LAMMPS_AL;
#define MORBUCKLJDSFT MORBUCKLJDSF<numtyp, acctyp>

extern Device<PRECISION,ACC_PRECISION> device;

template <class numtyp, class acctyp>
MORBUCKLJDSFT::MORBUCKLJDSF() : BaseCharge<numtyp,acctyp>(),
                                    _allocated(false) {
}

template <class numtyp, class acctyp>
MORBUCKLJDSFT::~MORBUCKLJDSF() {
  clear();
}

template <class numtyp, class acctyp>
int MORBUCKLJDSFT::bytes_per_atom(const int max_nbors) const {
  return this->bytes_per_atom_atomic(max_nbors);
}

template <class numtyp, class acctyp>
int MORBUCKLJDSFT::init(const int ntypes, double **host_cutsq, double **host_lj1,
                 double **host_lj2, double **host_lj3,  double **host_lj4,
                 double **host_offset,  double *host_special_lj,
                 const int nlocal, const int nall, const int max_nbors,
                 const int maxspecial, const double cell_size,
                 const double gpu_split, FILE *_screen,
                 double **host_cut_ljsq, const double host_cut_coulsq,
                 double *host_special_coul, const double qqrd2e,
                 const double e_shift, const double f_shift,
                 const double alpha, double **host_morse1,
                 double **host_r0, double **host_beta, double **host_d0,
                 double **host_rhoinv, double **host_buck1, double **host_buck2,
                 double **host_a, double **host_c) {
  int success;
  success=this->init_atomic(nlocal,nall,max_nbors,maxspecial,cell_size,gpu_split,
                            _screen,morse_buck_lj_dsf,"k_morse_buck_lj_dsf");
  if (success!=0)
    return success;

  _cut_coulsq=host_cut_coulsq;
  _e_shift=e_shift;
  _f_shift=f_shift;
  _alpha=alpha;

  // If atom type constants fit in shared memory use fast kernel
  int lj_types=ntypes;
  shared_types=false;
  int max_shared_types=this->device->max_shared_types();
  if (lj_types<=max_shared_types && this->_block_size>=max_shared_types) {
    lj_types=max_shared_types;
    shared_types=true;
  }
  _lj_types=lj_types;

  // Allocate a host write buffer for data initialization
  UCL_H_Vec<numtyp> host_write(lj_types*lj_types*32,*(this->ucl_device),
                               UCL_WRITE_ONLY);

  for (int i=0; i<lj_types*lj_types; i++)
    host_write[i]=0.0;

  lj1.alloc(lj_types*lj_types,*(this->ucl_device),UCL_READ_ONLY);
  this->atom->type_pack4(ntypes,lj_types,lj1,host_write,host_lj1,host_lj2,
                         host_cut_ljsq, host_cutsq);

  lj3.alloc(lj_types*lj_types,*(this->ucl_device),UCL_READ_ONLY);
  this->atom->type_pack4(ntypes,lj_types,lj3,host_write,host_lj3,host_lj4,
                         host_offset);

  mor1.alloc(lj_types*lj_types,*(this->ucl_device),UCL_READ_ONLY);
  this->atom->type_pack4(ntypes,lj_types,mor1,host_write,host_cutsq,host_morse1,
                         host_r0,host_beta);

  mor2.alloc(lj_types*lj_types,*(this->ucl_device),UCL_READ_ONLY);
  this->atom->type_pack2(ntypes,lj_types,mor2,host_write,host_d0,host_offset);

  coeff1.alloc(lj_types*lj_types,*(this->ucl_device),UCL_READ_ONLY);
  this->atom->type_pack4(ntypes,lj_types,coeff1,host_write,host_rhoinv,
                         host_buck1,host_buck2,host_cutsq);

  coeff2.alloc(lj_types*lj_types,*(this->ucl_device),UCL_READ_ONLY);
  this->atom->type_pack4(ntypes,lj_types,coeff2,host_write,host_a,host_c,
                         host_offset);

  
  sp_lj.alloc(8,*(this->ucl_device),UCL_READ_ONLY);
  for (int i=0; i<4; i++) {
    host_write[i]=host_special_lj[i];
    host_write[i+4]=host_special_coul[i];
  }
  ucl_copy(sp_lj,host_write,8,false);

  _qqrd2e=qqrd2e;

  _allocated=true;
  this->_max_bytes=lj1.row_bytes()+lj3.row_bytes()+sp_lj.row_bytes();
  return 0;
}

template <class numtyp, class acctyp>
void MORBUCKLJDSFT::clear() {
  if (!_allocated)
    return;
  _allocated=false;

  lj1.clear();
  lj3.clear();
  mor1.clear();
  mor2.clear();
  coeff1.clear();
  coeff2.clear();
  sp_lj.clear();
  this->clear_atomic();
}

template <class numtyp, class acctyp>
double MORBUCKLJDSFT::host_memory_usage() const {
  return this->host_memory_usage_atomic()+sizeof(MORBUCKLJDSF<numtyp,acctyp>);
}

// ---------------------------------------------------------------------------
// Calculate energies, forces, and torques
// ---------------------------------------------------------------------------
template <class numtyp, class acctyp>
void MORBUCKLJDSFT::loop(const bool _eflag, const bool _vflag) {
  // Compute the block size and grid size to keep all cores busy
  const int BX=this->block_size();
  int eflag, vflag;
  if (_eflag)
    eflag=1;
  else
    eflag=0;

  if (_vflag)
    vflag=1;
  else
    vflag=0;

  int GX=static_cast<int>(ceil(static_cast<double>(this->ans->inum())/
                               (BX/this->_threads_per_atom)));

  int ainum=this->ans->inum();
  int nbor_pitch=this->nbor->nbor_pitch();
  this->time_pair.start();
  if (shared_types) {
    this->k_pair_fast.set_size(GX,BX);
    this->k_pair_fast.run(&this->atom->x, &lj1, &lj3, &sp_lj,
                          &this->nbor->dev_nbor, &this->_nbor_data->begin(),
                          &this->ans->force, &this->ans->engv, &eflag,
                          &vflag, &ainum, &nbor_pitch, &this->atom->q,
                          &_cut_coulsq, &_qqrd2e, &_e_shift, &_f_shift, &_alpha,
                          &this->_threads_per_atom, &mor1, &mor2,
                          &coeff1, &coeff2);
  } else {
    this->k_pair.set_size(GX,BX);
    this->k_pair.run(&this->atom->x, &lj1, &lj3, &_lj_types, &sp_lj,
                     &this->nbor->dev_nbor, &this->_nbor_data->begin(),
                     &this->ans->force, &this->ans->engv,
                     &eflag, &vflag, &ainum, &nbor_pitch, &this->atom->q,
                     &_cut_coulsq, &_qqrd2e, &_e_shift, &_f_shift, &_alpha,
                     &this->_threads_per_atom, &mor1, &mor2,
                     &coeff1, &coeff2);
  }
  this->time_pair.stop();
}

template class MORBUCKLJDSF<PRECISION,ACC_PRECISION>;