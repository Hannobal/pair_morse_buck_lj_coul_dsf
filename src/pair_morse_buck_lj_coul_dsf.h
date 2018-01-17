/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef PAIR_CLASS

PairStyle(morse/buck/lj/coul/dsf,PairMorseBuckLJCoulDSF)

#else

#ifndef LMP_PAIR_MORSE_BUCK_LJ_COUL_DSF_H
#define LMP_PAIR_MORSE_BUCK_LJ_COUL_DSF_H

#include "pair.h"

namespace LAMMPS_NS {

class PairMorseBuckLJCoulDSF : public Pair {
 public:
  PairMorseBuckLJCoulDSF(class LAMMPS *);
  ~PairMorseBuckLJCoulDSF();
  void compute(int, int);
  void settings(int, char **);
  void coeff(int, char **);
  virtual void init_style();
  virtual double init_one(int, int);
  void write_restart(FILE *);
  void read_restart(FILE *);
  void write_restart_settings(FILE *);
  void read_restart_settings(FILE *);
  double single(int, int, int, int, double, double, double, double &);
  void *extract(const char *, int &);

 protected:
  double cut_lj_global;
  double **cut_lj,**cut_ljsq;
  double **epsilon,**sigma;
  double **lj1,**lj2,**lj3,**lj4,**offset;
  double **cut;
  double **d0,**beta,**r0;
  double **morse1;
  
  double **a,**rho,**c;
  double **rhoinv,**buck1,**buck2;

  double cut_coul,cut_coulsq;
  double alpha;
  double f_shift,e_shift;

  virtual void allocate();
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

E: Incorrect args for pair coefficients

Self-explanatory.  Check the input script or data file.

E: Pair style Morse/lj/coul/dsf requires atom attribute q

The atom style defined does not have these attributes.

*/
