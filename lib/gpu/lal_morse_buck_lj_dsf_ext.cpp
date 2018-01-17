/***************************************************************************
                             morse_buck_lj_dsf_ext.cpp
                             -------------------
                            W. Michael Brown (ORNL)

  Functions for LAMMPS access to lj/cut/coul/dsf acceleration routines.

 __________________________________________________________________________
    This file is part of the LAMMPS Accelerator Library (LAMMPS_AL)
 __________________________________________________________________________

    begin                : 7/12/2012
    email                : brownw@ornl.gov
 ***************************************************************************/

#include <iostream>
#include <cassert>
#include <math.h>

#include "lal_morse_buck_lj_dsf.h"

using namespace std;
using namespace LAMMPS_AL;

static MORBUCKLJDSF<PRECISION,ACC_PRECISION> MORBUCKLJDMF;

// ---------------------------------------------------------------------------
// Allocate memory on host and device and copy constants to device
// ---------------------------------------------------------------------------
int morse_buck_ljd_gpu_init(const int ntypes, double **cutsq, double **host_lj1,
                 double **host_lj2, double **host_lj3, double **host_lj4,
                 double **offset, double *special_lj, const int inum,
                 const int nall, const int max_nbors, const int maxspecial,
                 const double cell_size, int &gpu_mode, FILE *screen,
                 double **host_cut_ljsq, const double host_cut_coulsq,
                 double *host_special_coul, const double qqrd2e,
                 const double e_shift, const double f_shift,
                 const double alpha, double **host_mlj1, double **host_mlj2,
                 double **host_mlj3, double **host_mlj4, double **host_rhoinv,
                 double **host_buck1, double **host_buck2,
                 double **host_a, double **host_c) {
  MORBUCKLJDMF.clear();
  gpu_mode=MORBUCKLJDMF.device->gpu_mode();
  double gpu_split=MORBUCKLJDMF.device->particle_split();
  int first_gpu=MORBUCKLJDMF.device->first_device();
  int last_gpu=MORBUCKLJDMF.device->last_device();
  int world_me=MORBUCKLJDMF.device->world_me();
  int gpu_rank=MORBUCKLJDMF.device->gpu_rank();
  int procs_per_gpu=MORBUCKLJDMF.device->procs_per_gpu();

  MORBUCKLJDMF.device->init_message(screen,"lj/cut/coul/dsf",first_gpu,last_gpu);

  bool message=false;
  if (MORBUCKLJDMF.device->replica_me()==0 && screen)
    message=true;

  if (message) {
    fprintf(screen,"Initializing Device and compiling on process 0...");
    fflush(screen);
  }

  int init_ok=0;
  if (world_me==0)
    init_ok=MORBUCKLJDMF.init(ntypes, cutsq, host_lj1, host_lj2, host_lj3,
                       host_lj4, offset, special_lj, inum, nall, 300,
                       maxspecial, cell_size, gpu_split, screen, host_cut_ljsq,
                       host_cut_coulsq, host_special_coul, qqrd2e, e_shift,
                       f_shift, alpha, host_mlj1, host_mlj2, host_mlj3,
                       host_mlj4, host_rhoinv, host_buck1, host_buck2,
                       host_a, host_c);

  MORBUCKLJDMF.device->world_barrier();
  if (message)
    fprintf(screen,"Done.\n");

  for (int i=0; i<procs_per_gpu; i++) {
    if (message) {
      if (last_gpu-first_gpu==0)
        fprintf(screen,"Initializing Device %d on core %d...",first_gpu,i);
      else
        fprintf(screen,"Initializing Devices %d-%d on core %d...",first_gpu,
                last_gpu,i);
      fflush(screen);
    }
    if (gpu_rank==i && world_me!=0)
      init_ok=MORBUCKLJDMF.init(ntypes, cutsq, host_lj1, host_lj2, host_lj3, host_lj4,
                         offset, special_lj, inum, nall, 300, maxspecial,
                         cell_size, gpu_split, screen, host_cut_ljsq,
                         host_cut_coulsq, host_special_coul, qqrd2e, e_shift,
                         f_shift, alpha, host_mlj1, host_mlj2, host_mlj3,
                         host_mlj4, host_rhoinv, host_buck1, host_buck2,
                         host_a, host_c);

    MORBUCKLJDMF.device->gpu_barrier();
    if (message)
      fprintf(screen,"Done.\n");
  }
  if (message)
    fprintf(screen,"\n");

  if (init_ok==0)
    MORBUCKLJDMF.estimate_gpu_overhead();
  return init_ok;
}

void morse_buck_ljd_gpu_clear() {
  MORBUCKLJDMF.clear();
}

int** morse_buck_ljd_gpu_compute_n(const int ago, const int inum_full,
                        const int nall, double **host_x, int *host_type,
                        double *sublo, double *subhi, tagint *tag, int **nspecial,
                        tagint **special, const bool eflag, const bool vflag,
                        const bool eatom, const bool vatom, int &host_start,
                        int **ilist, int **jnum, const double cpu_time,
                        bool &success, double *host_q, double *boxlo,
                        double *prd) {
  return MORBUCKLJDMF.compute(ago, inum_full, nall, host_x, host_type, sublo,
                       subhi, tag, nspecial, special, eflag, vflag, eatom,
                       vatom, host_start, ilist, jnum, cpu_time, success,
                       host_q, boxlo, prd);
}

void morse_buck_ljd_gpu_compute(const int ago, const int inum_full, const int nall,
                     double **host_x, int *host_type, int *ilist, int *numj,
                     int **firstneigh, const bool eflag, const bool vflag,
                     const bool eatom, const bool vatom, int &host_start,
                     const double cpu_time, bool &success, double *host_q,
                     const int nlocal, double *boxlo, double *prd) {
  MORBUCKLJDMF.compute(ago,inum_full,nall,host_x,host_type,ilist,numj,firstneigh,eflag,
                vflag,eatom,vatom,host_start,cpu_time,success,host_q,
                nlocal,boxlo,prd);
}

double morse_buck_ljd_gpu_bytes() {
  return MORBUCKLJDMF.host_memory_usage();
}


