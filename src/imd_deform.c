
/******************************************************************************
*
* IMD -- The ITAP Molecular Dynamics Program
*
* Copyright 1996-2001 Institute for Theoretical and Applied Physics,
* University of Stuttgart, D-70550 Stuttgart
*
******************************************************************************/

/******************************************************************************
*
* imd_deform.c -- deform sample
*
******************************************************************************/

/******************************************************************************
* $Revision$
* $Date$
******************************************************************************/

#include "imd.h"

#ifdef HOMDEF   /* homogeneous deformation with pbc */

/*****************************************************************************
*
* expand_sample()
*
*****************************************************************************/

void expand_sample(void)
{
  int k;
  
  /* Apply expansion */
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (k=0; k<ncells; ++k) {
    int i;
    cell *p;
    p = cell_array + CELLS(k);
    for (i=0; i<p->n; ++i) {
      p->ort X(i) *= expansion.x;
      p->ort Y(i) *= expansion.y;
#ifndef TWOD
      p->ort Z(i) *= expansion.z;
#endif
    }
  }

  /* new box size */
#ifdef TWOD
  box_x.x *= expansion.x;  box_y.x *= expansion.x;
  box_x.y *= expansion.y;  box_y.y *= expansion.y;
#else
  box_x.x *= expansion.x;  box_x.y *= expansion.y;  box_x.z *= expansion.z;
  box_y.x *= expansion.x;  box_y.y *= expansion.y;  box_y.z *= expansion.z;
  box_z.x *= expansion.x;  box_z.y *= expansion.y;  box_z.z *= expansion.z;
#endif
  make_box();

  /* revise cell division if necessary */
  if ( (height.x < min_height.x) || (height.x > max_height.x)
    || (height.y < min_height.y) || (height.y > max_height.y)
#ifndef TWOD
    || (height.z < min_height.z) || (height.z > max_height.z)
#endif
  ) {
    init_cells();
    fix_cells();
  }  

} /* expand sample */


/*****************************************************************************
*
* shear_sample()
*
*****************************************************************************/

void shear_sample(void)
{
  int k;

  /* Apply shear */
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (k=0; k<ncells; ++k) {
    int i;
    cell *p;
    real tmport[2];
    p = cell_array + CELLS(k);
    for (i=0; i<p->n; ++i) {
      tmport[0]  = shear_factor.x * p->ort Y(i);
      tmport[1]  = shear_factor.y * p->ort X(i);
      p->ort X(i) += tmport[0];
      p->ort Y(i) += tmport[1];
    }
  }

  /* new box size */
  box_y.x += shear_factor.x * box_y.y;
  box_x.y += shear_factor.y * box_x.x;
  make_box();

  /* revise cell division if necessary */
  if ( (height.x < min_height.x) || (height.x > max_height.x)
    || (height.y < min_height.y) || (height.y > max_height.y)
#ifndef TWOD
    || (height.z < min_height.z) || (height.z > max_height.z)
#endif
  ) {
    init_cells();
    fix_cells();
  }  

} /* shear sample */

#endif /* HOMDEF */


#ifdef DEFORM

/*****************************************************************************
*
* deform_sample()
*
*****************************************************************************/

void deform_sample(void) 
{
  int k;
  /* loop over all atoms */
#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (k=0; k<ncells; ++k) {
    int i;
    cell *p;
    int sort;
    p = cell_array + CELLS(k);
    for (i=0; i<p->n; ++i) {
      sort = p->sorte[i];
      /* move particles with virtual types  */
      p->ort X(i) += (deform_shift + sort)->x;
      p->ort Y(i) += (deform_shift + sort)->y;
#ifndef TWOD
      p->ort Z(i) += (deform_shift + sort)->z;
#endif
    }
  }
}

#endif /* DEFORM */

