
/******************************************************************************
*
* imd_integrate -- various md integrators
*
******************************************************************************/

/******************************************************************************
* $Revision$
* $Date$
******************************************************************************/

#include "imd.h"

/*#define ATNR 2826  easier debuging... */

/*****************************************************************************
*
* Basic NVE Integrator
*
*****************************************************************************/

#ifdef NVE

void move_atoms_nve(void)
{
  int k;
  real tmpvec1[3], tmpvec2[3];
  static int count = 0;
  tot_kin_energy = 0.0;
  fnorm = 0.0;
  PxF   = 0.0;

  /* loop over all cells */
#ifdef _OPENMP
#pragma omp parallel for reduction(+:tot_kin_energy,fnorm,PxF)
#endif
  for (k=0; k<ncells; ++k) {

    int  i, sort;
    cell *p;
    real kin_energie_1, kin_energie_2, tmp;
#ifdef UNIAX    
    real rot_energie_1, rot_energie_2;
    real dot, norm;
    vektor cross;
#endif
    p = cell_array + CELLS(k);

#ifdef PVPCRAY
#pragma ivdep
#endif
#ifdef SX4
#pragma vdir vector,nodep
#endif
    for (i=0; i<p->n; ++i) {

        kin_energie_1 = SPRODN(p->impuls,i,p->impuls,i);
#ifdef UNIAX
        rot_energie_1 = SPRODN(p->dreh_impuls,i,p->dreh_impuls,i);
#endif

        sort = VSORTE(p,i);
#ifdef FBC
        /* give virtual particles their extra force */
	p->kraft X(i) += (fbc_forces + sort)->x;
	p->kraft Y(i) += (fbc_forces + sort)->y;
#ifndef TWOD
	p->kraft Z(i) += (fbc_forces + sort)->z;
#endif

#endif
	/* and set their force (->momentum) in restricted directions to 0 */
	p->kraft X(i) *= (restrictions + sort)->x;
	p->kraft Y(i) *= (restrictions + sort)->y;
#ifndef TWOD
	p->kraft Z(i) *= (restrictions + sort)->z;
#endif

#ifdef FNORM
	fnorm +=  SPRODN(p->kraft,i,p->kraft,i);
#endif

	p->impuls X(i) += timestep * p->kraft X(i);
        p->impuls Y(i) += timestep * p->kraft Y(i);
#ifndef TWOD
        p->impuls Z(i) += timestep * p->kraft Z(i);
#endif

	/* "Globale Konvergenz": like mik, just with the global 
           force and momentum vectors */
#ifdef GLOK              
	PxF   +=  SPRODN(p->impuls,i,p->kraft,i);
#endif

#ifdef UNIAX
        dot = 2.0 * SPRODN(p->dreh_impuls,i,p->achse,i);
        p->dreh_impuls X(i) += timestep * p->dreh_moment X(i)
                               - dot * p->achse X(i);
        p->dreh_impuls Y(i) += timestep * p->dreh_moment Y(i)
                               - dot * p->achse Y(i);
        p->dreh_impuls Z(i) += timestep * p->dreh_moment Z(i)
                               - dot * p->achse Z(i);
#endif

        kin_energie_2 = SPRODN(p->impuls,i,p->impuls,i);
#ifdef UNIAX
        rot_energie_2 = SPRODN(p->dreh_impuls,i,p->dreh_impuls,i);
#endif
        /* sum up kinetic energy on this CPU */
        tot_kin_energy += (kin_energie_1 + kin_energie_2) / (4 * MASSE(p,i));
#ifdef UNIAX
        tot_kin_energy += (rot_energie_1 + rot_energie_2) 
                                    / (4 * p->traeg_moment[i]);	  
#endif	  

        /* new positions */
        tmp = timestep / MASSE(p,i);
        p->ort X(i) += tmp * p->impuls X(i);
        p->ort Y(i) += tmp * p->impuls Y(i);
#ifndef TWOD
        p->ort Z(i) += tmp * p->impuls Z(i);
#endif

#ifdef SHOCK
	if (shock_mode == 3) {
	  if (p->ort X(i) > box_x.x) p->impuls X(i) = -p->impuls X(i);
	}
#endif

        /* new molecular axes */
#ifdef UNIAX
        cross.x = p->dreh_impuls Y(i) * p->achse Z(i)
                - p->dreh_impuls Z(i) * p->achse Y(i);
        cross.y = p->dreh_impuls Z(i) * p->achse X(i)
                - p->dreh_impuls X(i) * p->achse Z(i);
        cross.z = p->dreh_impuls X(i) * p->achse Y(i)
                - p->dreh_impuls Y(i) * p->achse X(i);

        p->achse X(i) += timestep * cross.x / p->traeg_moment[i];
        p->achse Y(i) += timestep * cross.y / p->traeg_moment[i];
        p->achse Z(i) += timestep * cross.z / p->traeg_moment[i];

        norm = sqrt( SPRODN(p->achse,i,p->achse,i) );
	    
        p->achse X(i) /= norm;
        p->achse Y(i) /= norm;
        p->achse Z(i) /= norm;
#endif    

#ifdef STRESS_TENS
#ifdef SHOCK
	/* plate against bulk */
        if (shock_mode == 1) {
          if ( p->ort X(i) < shock_strip )
            p->presstens X(i) += (p->impuls X(i) - shock_speed * MASSE(p,i)) 
                    * (p->impuls X(i) - shock_speed * MASSE(p,i)) / MASSE(p,i);
          else
	    p->presstens X(i) += p->impuls X(i) * p->impuls X(i)/MASSE(p,i);
        }
	/* two halves against one another */
        if (shock_mode == 2) {
          if ( p->ort X(i) < box_x.x*0.5 )
            p->presstens X(i) += (p->impuls X(i) - shock_speed * MASSE(p,i)) 
                    * (p->impuls X(i) - shock_speed * MASSE(p,i)) / MASSE(p,i);
          else
            p->presstens X(i) += (p->impuls X(i) + shock_speed * MASSE(p,i)) 
                    * (p->impuls X(i) + shock_speed * MASSE(p,i)) / MASSE(p,i);
        }
	/* bulk against wall */
        if (shock_mode == 3) p->presstens X(i) += (p->impuls X(i) - 
	  shock_speed * MASSE(p,i)) * (p->impuls X(i) - shock_speed * 
           MASSE(p,i)) / MASSE(p,i);
#else
        p->presstens X(i)        += p->impuls X(i) * p->impuls X(i)/MASSE(p,i);
#endif
        p->presstens Y(i)        += p->impuls Y(i) * p->impuls Y(i)/MASSE(p,i);
#ifdef TWOD
        p-> presstens_offdia[i]  += p->impuls X(i) * p->impuls Y(i)/MASSE(p,i);
#else
        p->presstens        Z(i) += p->impuls Z(i) * p->impuls Z(i)/MASSE(p,i);
        p->presstens_offdia X(i) += p->impuls Y(i) * p->impuls Z(i)/MASSE(p,i);
        p->presstens_offdia Y(i) += p->impuls Z(i) * p->impuls X(i)/MASSE(p,i);
        p->presstens_offdia Z(i) += p->impuls X(i) * p->impuls Y(i)/MASSE(p,i);
#endif
#endif /* STRESS_TENS */
    }
  }

#ifdef MPI
  /* add up results from different CPUs */
  tmpvec1[0] = tot_kin_energy;
  tmpvec1[1] = fnorm;
  tmpvec1[2] = PxF;

  MPI_Allreduce( tmpvec1, tmpvec2, 3, REAL, MPI_SUM, cpugrid);

  tot_kin_energy = tmpvec2[0];
  fnorm          = tmpvec2[1];
  PxF            = tmpvec2[2];
#endif
  
#ifdef AND
  /* Andersen Thermostat -- Initialize the velocities now and then */
  ++count;
  if ((tmp_interval!=0) && (0==count%tmp_interval)) maxwell(temperature);
#endif


}

#else

void move_atoms_nve(void) 
{
  if (myid==0)
  error("the chosen ensemble NVE is not supported by this binary");
}

#endif


/*****************************************************************************
*
*  NVE Integrator with microconvergence relaxation
*
*****************************************************************************/

#ifdef MIK

void move_atoms_mik(void)
{
  int k;
  real tmpvec1[2], tmpvec2[2];
  static int count = 0;
  tot_kin_energy = 0.0;
  fnorm = 0.0;

#ifdef AND
  /* Andersen Thermostat -- Initialize the velocities now and then */
  ++count;
  if ((tmp_interval!=0) && (0==count%tmp_interval)) maxwell(temperature);
#endif

  /* loop over all cells */
#ifdef _OPENMP
#pragma omp parallel for reduction(+:tot_kin_energy,fnorm)
#endif
  for (k=0; k<ncells; ++k) {

    int  i, sort;
    cell *p;
    real kin_energie_1, kin_energie_2, tmp;
    p = cell_array + CELLS(k);

#ifdef PVPCRAY
#pragma ivdep
#endif
#ifdef SX4
#pragma vdir vector,nodep
#endif
    for (i=0; i<p->n; ++i) {

        kin_energie_1 = SPRODN(p->impuls,i,p->impuls,i);

	sort = VSORTE(p,i);
#ifdef FBC
#ifdef ATNR /* debugging  stuff */
	if(p->nummer[i]==ATNR) {
          printf("kraft vorher: %.16f  %.16f  %.16f \n",
                 p->kraft X(i),p->kraft Y(i),p->kraft Z(i));
          printf("fbc_forces: %.16f  %.16f  %.16f \n", 
                 (fbc_forces + sort)->x,(fbc_forces + sort)->y,
                 (fbc_forces + sort)->z);
          fflush(stdout);
	}
#endif
        /* give virtual particles their extra force */
	p->kraft X(i) += (fbc_forces + sort)->x;
	p->kraft Y(i) += (fbc_forces + sort)->y;
#ifndef TWOD
	p->kraft Z(i) += (fbc_forces + sort)->z;
#endif
#endif /* FBC */

	/* and set their force (->momentum) in restricted directions to 0 */
	p->kraft X(i) *= (restrictions + sort)->x;
	p->kraft Y(i) *= (restrictions + sort)->y;
#ifndef TWOD
	p->kraft Z(i) *= (restrictions + sort)->z;
#endif
	
#ifdef FBC
#ifdef ATNR
	if(p->nummer[i]==ATNR) {
          printf("kraft nachher: %.16f  %.16f  %.16f \n",
                 p->kraft X(i),p->kraft Y(i),p->kraft Z(i));
          fflush(stdout);
          printf("impuls  vorher: %.16f  %.16f  %.16f \n",
                 p->impuls X(i),p->impuls Y(i),p->impuls Z(i));
          fflush(stdout);
        }
#endif
#endif  /* FBC */

#ifdef FNORM
	fnorm +=  SPRODN(p->kraft,i,p->kraft,i);
#endif

        p->impuls X(i) += timestep * p->kraft X(i);
        p->impuls Y(i) += timestep * p->kraft Y(i);
#ifndef TWOD
        p->impuls Z(i) += timestep * p->kraft Z(i);
#endif

	/* Mikroconvergence Algorithm - set velocity zero if a*v < 0 */
	if (0.0 > SPRODN(p->impuls,i,p->kraft,i)) {
          p->impuls X(i) = 0.0;
          p->impuls Y(i) = 0.0;
#ifndef TWOD
          p->impuls Z(i) = 0.0;
#endif
        } else { /* new positions */
          tmp = timestep / MASSE(p,i);
          p->ort X(i) += tmp * p->impuls X(i);
          p->ort Y(i) += tmp * p->impuls Y(i);
#ifndef TWOD
          p->ort Z(i) += tmp * p->impuls Z(i);
#endif
        }

#ifdef ATNR
        if(p->nummer[i]==ATNR) {
          printf("impuls: %.16f  %.16f  %.16f \n",
                 p->impuls X(i),p->impuls Y(i),p->impuls Z(i));
          printf("ort:  %.16f  %.16f  %.16f \n",
                 p->ort X(i),p->ort Y(i),p->ort Z(i));
          fflush(stdout);
        }
#endif
        kin_energie_2 =  SPRODN(p->impuls,i,p->impuls,i);

        /* sum up kinetic energy on this CPU */ 
        tot_kin_energy += (kin_energie_1 + kin_energie_2) / (4.0 * MASSE(p,i));

#ifdef STRESS_TENS
        p->presstens        X(i) += p->impuls X(i) * p->impuls X(i)/MASSE(p,i);
        p->presstens        Y(i) += p->impuls Y(i) * p->impuls Y(i)/MASSE(p,i);
#ifdef TWOD
        p->presstens_offdia[i]   += p->impuls X(i) * p->impuls Y(i)/MASSE(p,i);
#else
        p->presstens Z(i)        += p->impuls Z(i) * p->impuls Z(i)/MASSE(p,i);
        p->presstens_offdia X(i) += p->impuls Y(i) * p->impuls Z(i)/MASSE(p,i);
        p->presstens_offdia Y(i) += p->impuls Z(i) * p->impuls X(i)/MASSE(p,i);
        p->presstens_offdia Z(i) += p->impuls X(i) * p->impuls Y(i)/MASSE(p,i);
#endif
#endif
    }
  }

#ifdef MPI
  /* add up results from different CPUs */
  tmpvec1[0] = tot_kin_energy;
  tmpvec1[1] = fnorm;

  MPI_Allreduce( tmpvec1, tmpvec2, 2, REAL, MPI_SUM, cpugrid);

  tot_kin_energy = tmpvec2[0];
  fnorm          = tmpvec2[1];
#endif

}

#else

void move_atoms_mik(void) 
{
  if (myid==0)
  error("the chosen ensemble MIK is not supported by this binary");
}

#endif


/*****************************************************************************
*
* NVT Integrator with Nose Hoover Thermostat 
*
*****************************************************************************/

#ifdef NVT

void move_atoms_nvt(void)

{
  int k;
  real tmpvec1[3], tmpvec2[3], ttt;
  real E_kin_1 = 0.0, E_kin_2 = 0.0;
  real reibung, eins_d_reib;
  real E_rot_1 = 0.0, E_rot_2 = 0.0;
#ifdef UNIAX
  real reibung_rot,  eins_d_reib_rot;
#endif
#ifdef SLLOD
  ivektor max_cell_dim;
#endif
  fnorm = 0.0;

  reibung     =        1.0 - eta * inv_tau_eta * timestep / 2.0;
  eins_d_reib = 1.0 / (1.0 + eta * inv_tau_eta * timestep / 2.0);
#ifdef UNIAX
  reibung_rot     =        1.0 - eta_rot * inv_tau_eta_rot * timestep / 2.0;
  eins_d_reib_rot = 1.0 / (1.0 + eta_rot * inv_tau_eta_rot * timestep / 2.0);
#endif
   
#ifdef _OPENMP
#pragma omp parallel for reduction(+:E_kin_1,E_kin_2,E_rot_1,E_rot_2,fnorm)
#endif
  for (k=0; k<ncells; ++k) {

    int i;
    int sort;
    cell *p;
    real tmp;
#ifdef UNIAX
    real dot, norm ;
    vektor cross ;
#endif
    p = cell_array + CELLS(k);

    for (i=0; i<p->n; ++i) {

        /* twice the old kinetic energy */
        E_kin_1 +=  SPRODN(p->impuls,i,p->impuls,i) / MASSE(p,i);
#ifdef UNIAX
        E_rot_1 +=  SPRODN(p->dreh_impuls,i,p->dreh_impuls,i) 
                                                  / p->traeg_moment[i];
#endif

	sort = VSORTE(p,i);

	p->kraft X(i) *= (restrictions + sort)->x;
	p->kraft Y(i) *= (restrictions + sort)->y;
#ifndef TWOD
	p->kraft Z(i) *= (restrictions + sort)->z;
#endif
#ifdef FNORM
	fnorm +=  SPRODN(p->kraft,i,p->kraft,i);
#endif

	p->impuls X(i) = (p->impuls X(i) * reibung + timestep * p->kraft X(i)) 
                           * eins_d_reib * (restrictions + sort)->x;
        p->impuls Y(i) = (p->impuls Y(i) * reibung + timestep * p->kraft Y(i)) 
                           * eins_d_reib * (restrictions + sort)->y;
#ifndef TWOD
        p->impuls Z(i) = (p->impuls Z(i) * reibung + timestep * p->kraft Z(i)) 
                           * eins_d_reib * (restrictions + sort)->z;
#endif

#ifdef SLLOD
	p->impuls X(i) += epsilon * p->impuls Y(i) / MASSE(p,i);
#endif

#ifdef UNIAX
        /* new angular momenta */
        dot = 2.0 * SPRODN(p->dreh_impuls,i,p->achse,i);

        p->dreh_impuls X(i) = eins_d_reib_rot
            * ( p->dreh_impuls X(i) * reibung_rot
                + timestep * p->dreh_moment X(i) - dot * p->achse X(i) );
        p->dreh_impuls Y(i) = eins_d_reib_rot
            * ( p->dreh_impuls Y(i) * reibung_rot
                + timestep * p->dreh_moment Y(i) - dot * p->achse Y(i) );
        p->dreh_impuls Z(i) = eins_d_reib_rot
            * ( p->dreh_impuls Z(i) * reibung_rot
                + timestep * p->dreh_moment Z(i) - dot * p->achse Z(i) );
#endif

        /* twice the new kinetic energy */ 
        E_kin_2 += SPRODN(p->impuls,i,p->impuls,i) / MASSE(p,i);
#ifdef UNIAX
        E_rot_2 += SPRODN(p->dreh_impuls,i,p->dreh_impuls,i) 
                                                     / p->traeg_moment[i];
#endif

        /* new positions */
        tmp = timestep / MASSE(p,i);
        p->ort X(i) += tmp * p->impuls X(i);
        p->ort Y(i) += tmp * p->impuls Y(i);
#ifndef TWOD
        p->ort Z(i) += tmp * p->impuls Z(i);
#endif

#ifdef SLLOD
	p->ort X(i) += epsilon * p->ort Y(i);
#endif /* SLLOD */

#ifdef UNIAX
        cross.x = p->dreh_impuls Y(i) * p->achse Z(i)
                - p->dreh_impuls Z(i) * p->achse Y(i);
        cross.y = p->dreh_impuls Z(i) * p->achse X(i)
                - p->dreh_impuls X(i) * p->achse Z(i);
        cross.z = p->dreh_impuls X(i) * p->achse Y(i)
                - p->dreh_impuls Y(i) * p->achse X(i);

        p->achse X(i) += timestep * cross.x / p->traeg_moment[i];
        p->achse Y(i) += timestep * cross.y / p->traeg_moment[i];
        p->achse Z(i) += timestep * cross.z / p->traeg_moment[i];

        norm = sqrt( SPRODN(p->achse,i,p->achse,i) );

        p->achse X(i) /= norm;
        p->achse Y(i) /= norm;
        p->achse Z(i) /= norm;
#endif

#ifdef STRESS_TENS
        p->presstens        X(i) += p->impuls X(i) * p->impuls X(i)/MASSE(p,i);
        p->presstens        Y(i) += p->impuls Y(i) * p->impuls Y(i)/MASSE(p,i);
#ifdef TWOD
        p->presstens_offdia[i]   += p->impuls X(i) * p->impuls Y(i)/MASSE(p,i);
#else
        p->presstens Z(i)        += p->impuls Z(i) * p->impuls Z(i)/MASSE(p,i);
        p->presstens_offdia X(i) += p->impuls Y(i) * p->impuls Z(i)/MASSE(p,i);
        p->presstens_offdia Y(i) += p->impuls Z(i) * p->impuls X(i)/MASSE(p,i);
        p->presstens_offdia Z(i) += p->impuls X(i) * p->impuls Y(i)/MASSE(p,i);
#endif
#endif
    }
  }
  
#ifdef SLLOD
  /* new box size */
  box_y.x += epsilon * box_y.y;
  make_box();

  /* revise cell decomposition if necessary */
  max_cell_dim = maximal_cell_dim();
  if ((max_cell_dim.x<global_cell_dim.x) || (max_cell_dim.y<global_cell_dim.y)
#ifndef TWOD
      || (max_cell_dim.z<global_cell_dim.z)
#endif
      ) {
    init_cells();
    fix_cells();
  }
#endif

#ifdef UNIAX
  tot_kin_energy = ( E_kin_1 + E_kin_2 + E_rot_1 + E_rot_2 ) / 4.0;
#else
  tot_kin_energy = ( E_kin_1 + E_kin_2 ) / 4.0;
#endif

#ifdef MPI
  /* add up results from different CPUs */
  tmpvec1[0] = tot_kin_energy;
  tmpvec1[1] = E_kin_2;
  tmpvec1[2] = E_rot_2;

  MPI_Allreduce( tmpvec1, tmpvec2, 3, REAL, MPI_SUM, cpugrid);

  tot_kin_energy = tmpvec2[0];
  E_kin_2        = tmpvec2[1];
  E_rot_2        = tmpvec2[2];
#endif

  /* time evolution of constraints */
  ttt  = nactive * temperature;
  eta += timestep * (E_kin_2 / ttt - 1.0) * inv_tau_eta;
#ifdef UNIAX
  ttt  = nactive_rot * temperature;
  eta_rot += timestep * ( E_rot_2 / ttt - 1.0 ) * inv_tau_eta_rot;
#endif
  
}

#else

void move_atoms_nvt(void) 
{
  if (myid==0)
  error("the chosen ensemble NVT is not supported by this binary");
}

#endif


/******************************************************************************
*
* NPT Integrator with Nose Hoover Thermostat
*
******************************************************************************/

#ifdef NPT_iso

void move_atoms_npt_iso(void)

{
  int  k;
  real Ekin_old = 0.0, Ekin_new = 0.0;
  real fric, ifric, tmpvec1[4], tmpvec2[4], ttt;
  real Erot_old = 0.0, Erot_new = 0.0;
  real reib, ireib ;

  /* relative box size change */
  box_size.x      += 2.0 * timestep * xi.x * inv_tau_xi;
  actual_shrink.x *= box_size.x;
  fric  =      1.0 - (xi.x * inv_tau_xi + eta * inv_tau_eta) * timestep / 2.0;
  ifric = 1.0/(1.0 + (xi.x * inv_tau_xi + eta * inv_tau_eta) * timestep / 2.0);
#ifdef UNIAX
  reib  =      1.0 - eta_rot * inv_tau_eta_rot * timestep / 2.0;
  ireib = 1.0/(1.0 + eta_rot * inv_tau_eta_rot * timestep / 2.0);
#endif

  /* loop over all cells */
#ifdef _OPENMP
#pragma omp parallel for reduction(+:Ekin_old,Ekin_new,Erot_old,Erot_new)
#endif
  for (k=0; k<ncells; ++k) {

    int i;
    cell *p;
    real tmp;
#ifdef UNIAX
    real dot, norm ;
    vektor cross ;
#endif
    p = cell_array + CELLS(k);

    for (i=0; i<p->n; ++i) {

      /* twice the old kinetic energy */ 
      Ekin_old += SPRODN(p->impuls,i,p->impuls,i) / MASSE(p,i);
#ifdef UNIAX
      Erot_old += SPRODN(p->dreh_impuls,i,p->dreh_impuls,i) 
                                              / p->traeg_moment[i];
#endif

      /* new momenta */
      p->impuls X(i) = (fric*p->impuls X(i)+timestep*p->kraft X(i))*ifric;
      p->impuls Y(i) = (fric*p->impuls Y(i)+timestep*p->kraft Y(i))*ifric;
#ifndef TWOD
      p->impuls Z(i) = (fric*p->impuls Z(i)+timestep*p->kraft Z(i))*ifric;
#endif

#ifdef UNIAX
      /* new angular momenta */
      dot = 2.0 * SPRODN(p->dreh_impuls,i,p->achse,i);
      p->dreh_impuls X(i) = ireib * ( p->dreh_impuls X(i) * reib 
              + timestep * p->dreh_moment X(i) - dot * p->achse X(i) );
      p->dreh_impuls Y(i) = ireib * ( p->dreh_impuls Y(i) * reib 
              + timestep * p->dreh_moment Y(i) - dot * p->achse Y(i) );
      p->dreh_impuls Z(i) = ireib * ( p->dreh_impuls Z(i) * reib 
              + timestep * p->dreh_moment Z(i) - dot * p->achse Z(i) );
#endif

      /* twice the new kinetic energy */ 
      Ekin_new += SPRODN(p->impuls,i,p->impuls,i) / MASSE(p,i);
#ifdef UNIAX
      Erot_new += SPRODN(p->dreh_impuls,i,p->dreh_impuls,i) 
                                                   / p->traeg_moment[i];
#endif

      /* new positions */
      tmp = p->impuls X(i) * (1.0 + box_size.x) / (2.0 * MASSE(p,i));
      p->ort X(i) = box_size.x * ( p->ort X(i) + timestep * tmp );
      tmp = p->impuls Y(i) * (1.0 + box_size.x) / (2.0 * MASSE(p,i));
      p->ort Y(i) = box_size.x * ( p->ort Y(i) + timestep * tmp );
#ifndef TWOD
      tmp = p->impuls Z(i) * (1.0 + box_size.x) / (2.0 * MASSE(p,i));
      p->ort Z(i) = box_size.x * ( p->ort Z(i) + timestep * tmp );
#endif

#ifdef UNIAX
      /* new molecular axes */
      cross.x = p->dreh_impuls Y(i) * p->achse Z(i)
              - p->dreh_impuls Z(i) * p->achse Y(i);
      cross.y = p->dreh_impuls Z(i) * p->achse X(i)
              - p->dreh_impuls X(i) * p->achse Z(i);
      cross.z = p->dreh_impuls X(i) * p->achse Y(i)
              - p->dreh_impuls Y(i) * p->achse X(i);

      p->achse X(i) += timestep * cross.x / p->traeg_moment[i];
      p->achse Y(i) += timestep * cross.y / p->traeg_moment[i];
      p->achse Z(i) += timestep * cross.z / p->traeg_moment[i];

      norm = sqrt( SPRODN(p->achse,i,p->achse,i) );

      p->achse X(i) /= norm;
      p->achse Y(i) /= norm;
      p->achse Z(i) /= norm;
#endif

#ifdef STRESS_TENS
      p->presstens        X(i) += p->impuls X(i) * p->impuls X(i) / MASSE(p,i);
      p->presstens        Y(i) += p->impuls Y(i) * p->impuls Y(i) / MASSE(p,i);
#ifdef TWOD
      p->presstens_offdia[i]   += p->impuls X(i) * p->impuls Y(i) / MASSE(p,i);
#else
      p->presstens Z(i)        += p->impuls Z(i) * p->impuls Z(i) / MASSE(p,i);
      p->presstens_offdia X(i) += p->impuls Y(i) * p->impuls Z(i) / MASSE(p,i);
      p->presstens_offdia Y(i) += p->impuls Z(i) * p->impuls X(i) / MASSE(p,i);
      p->presstens_offdia Z(i) += p->impuls X(i) * p->impuls Y(i) / MASSE(p,i);
#endif
#endif

    }
  }

#ifdef MPI
  /* add up results from all CPUs */
  tmpvec1[0] = Ekin_old;
  tmpvec1[1] = Ekin_new;
  tmpvec1[2] = Erot_old;
  tmpvec1[3] = Erot_new;

  MPI_Allreduce( tmpvec1, tmpvec2, 4, REAL, MPI_SUM, cpugrid);

  Ekin_old = tmpvec2[0];
  Ekin_new = tmpvec2[1];
  Erot_old = tmpvec2[2];
  Erot_new = tmpvec2[3];
#endif

#ifdef UNIAX
  tot_kin_energy = ( Ekin_old + Ekin_new 
		     + Erot_old + Erot_new ) / 4.0;
  pressure = ( 2.0 / 5.0 * tot_kin_energy + virial / 3.0 ) / volume ;
#else
  tot_kin_energy = ( Ekin_old + Ekin_new ) / 4.0;
  pressure = ( 2.0 * tot_kin_energy + virial ) / ( DIM * volume ) ;
#endif

  /* time evolution of constraints */
  ttt  = nactive * temperature;
  eta += timestep * (Ekin_new / ttt - 1.0) * inv_tau_eta;
#ifdef UNIAX
  ttt  = nactive_rot * temperature;
  eta_rot += timestep * (Erot_new / ttt - 1.0) * inv_tau_eta_rot;
#endif

  ttt = xi_old.x + timestep * 2.0 * (pressure - pressure_ext.x) * volume
                          * inv_tau_xi * DIM / (nactive * temperature);
  xi_old.x = xi.x;
  xi.x = ttt;

  /* new box size */
  box_x.x *= box_size.x;  tbox_x.x /= box_size.x;
  box_x.y *= box_size.x;  tbox_x.x /= box_size.x;
  box_y.x *= box_size.x;  tbox_y.x /= box_size.x;
  box_y.y *= box_size.x;  tbox_y.x /= box_size.x;
#ifndef TWOD
  box_x.z *= box_size.x;  tbox_x.z /= box_size.x;
  box_y.z *= box_size.x;  tbox_y.z /= box_size.x;
  box_z.x *= box_size.x;  tbox_z.x /= box_size.x;
  box_z.y *= box_size.x;  tbox_z.y /= box_size.x;
  box_z.z *= box_size.x;  tbox_z.z /= box_size.x;
#endif  

  /* old box_size relative to the current one, which is set to 1.0 */
  box_size.x = 1.0 / box_size.x;

  /* check whether the box has not changed too much */
  if (actual_shrink.x < limit_shrink.x) {
     cells_too_small = 1;
  }
  if ((actual_shrink.x < limit_shrink.x) 
       || (actual_shrink.x > limit_growth.x)) {
     revise_cell_division = 1;
  }
}

#else

void move_atoms_npt_iso(void) 
{
  if (myid==0)
  error("the chosen ensemble NPT_ISO is not supported by this binary");
}

#endif


/******************************************************************************
*
*  NPT Integrator with Nose Hoover Thermostat
*
******************************************************************************/

#ifdef NPT_axial

void move_atoms_npt_axial(void)

{
  int k;
  real Ekin, ttt, xi_tmp, tmpvec1[4], tmpvec2[4];
  vektor fric, ifric;

  /* initialize data, and compute new box size */
  Ekin             = 0.0;
  stress_x         = 0.0;
  stress_y         = 0.0;
  /* relative box size change */ 
  box_size.x      += 2.0 * timestep * xi.x * inv_tau_xi;  
  box_size.y      += 2.0 * timestep * xi.y * inv_tau_xi;
  actual_shrink.x *= box_size.x;
  actual_shrink.y *= box_size.y;
  fric.x  =    1.0 - (xi.x * inv_tau_xi + eta * inv_tau_eta) * timestep / 2.0;
  fric.y  =    1.0 - (xi.y * inv_tau_xi + eta * inv_tau_eta) * timestep / 2.0;
  ifric.x = 1/(1.0 + (xi.x * inv_tau_xi + eta * inv_tau_eta) * timestep / 2.0);
  ifric.y = 1/(1.0 + (xi.y * inv_tau_xi + eta * inv_tau_eta) * timestep / 2.0);
#ifndef TWOD
  stress_z         = 0.0;
  box_size.z      += 2.0 * timestep * xi.z * inv_tau_xi;  
  actual_shrink.z *= box_size.z;
  fric.z  =    1.0 - (xi.z * inv_tau_xi + eta * inv_tau_eta) * timestep / 2.0;
  ifric.z = 1/(1.0 + (xi.z * inv_tau_xi + eta * inv_tau_eta) * timestep / 2.0);
#endif

  /* loop over all cells */
#ifdef _OPENMP
#pragma omp parallel for reduction(+:Ekin,stress_x,stress_y,stress_z)
#endif
  for (k=0; k<ncells; ++k) {

    int i;
    cell *p;
    real tmp;
    p = cell_array + CELLS(k);

    /* loop over atoms in cell */
    for (i=0; i<p->n; ++i) {

      /* contribution of old p's to stress */
      stress_x += p->impuls X(i) * p->impuls X(i) / MASSE(p,i);
      stress_y += p->impuls Y(i) * p->impuls Y(i) / MASSE(p,i);
#ifndef TWOD
      stress_z += p->impuls Z(i) * p->impuls Z(i) / MASSE(p,i);
#endif

      /* new momenta */
      p->impuls X(i) = (fric.x * p->impuls X(i)
                                   + timestep * p->kraft X(i)) * ifric.x;
      p->impuls Y(i) = (fric.y * p->impuls Y(i)
                                   + timestep * p->kraft Y(i)) * ifric.y;
#ifndef TWOD
      p->impuls Z(i) = (fric.z * p->impuls Z(i)
                                   + timestep * p->kraft Z(i)) * ifric.z;
#endif

      /* new kinetic energy, and contribution of new p's to stress */
      tmp       = p->impuls X(i) * p->impuls X(i) / MASSE(p,i);
      stress_x += tmp;
      Ekin     += tmp;
      tmp       = p->impuls Y(i) * p->impuls Y(i) / MASSE(p,i);
      stress_y += tmp;
      Ekin     += tmp;
#ifndef TWOD
      tmp       = p->impuls Z(i) * p->impuls Z(i) / MASSE(p,i);
      stress_z += tmp;
      Ekin     += tmp;
#endif
	  
      /* new positions */
      tmp = p->impuls X(i) * (1.0 + box_size.x) / (2.0 * MASSE(p,i));
      p->ort X(i) = box_size.x * (p->ort X(i) + timestep * tmp);
      tmp = p->impuls Y(i) * (1.0 + box_size.y) / (2.0 * MASSE(p,i));
      p->ort Y(i) = box_size.y * (p->ort Y(i) + timestep * tmp);
#ifndef TWOD
      tmp = p->impuls Z(i) * (1.0 + box_size.z) / (2.0 * MASSE(p,i));
      p->ort Z(i) = box_size.z * (p->ort Z(i) + timestep * tmp);
#endif

#ifdef STRESS_TENS
      p->presstens        X(i) += p->impuls X(i) * p->impuls X(i) / MASSE(p,i);
      p->presstens        Y(i) += p->impuls Y(i) * p->impuls Y(i) / MASSE(p,i);
#ifdef TWOD
      p->presstens_offdia[i]   += p->impuls X(i) * p->impuls Y(i) / MASSE(p,i);
#else
      p->presstens Z(i)        += p->impuls Z(i) * p->impuls Z(i) / MASSE(p,i);
      p->presstens_offdia X(i) += p->impuls Y(i) * p->impuls Z(i) / MASSE(p,i);
      p->presstens_offdia Y(i) += p->impuls Z(i) * p->impuls X(i) / MASSE(p,i);
      p->presstens_offdia Z(i) += p->impuls X(i) * p->impuls Y(i) / MASSE(p,i);
#endif
#endif
    }
  }

#ifdef MPI
  /* add up results from different CPUs */
  tmpvec1[0] = Ekin;
  tmpvec1[1] = stress_x;
  tmpvec1[2] = stress_y;
#ifndef TWOD
  tmpvec1[3] = stress_z;
#endif

  MPI_Allreduce( tmpvec1, tmpvec2, 1+DIM, REAL, MPI_SUM, cpugrid);

  Ekin     = tmpvec2[0];
  stress_x = tmpvec2[1];
  stress_y = tmpvec2[2];
#ifndef TWOD
  stress_z = tmpvec2[3];
#endif
#endif

  tot_kin_energy  = stress_x + stress_y;
  stress_x        = (stress_x / 2.0 + vir_x) / volume;
  stress_y        = (stress_y / 2.0 + vir_y) / volume;
#ifndef TWOD
  tot_kin_energy += stress_z;
  stress_z        = (stress_z / 2.0 + vir_z) / volume;
#endif
  tot_kin_energy /= 4.0;

  /* update parameters */
  ttt  = nactive * temperature;
  eta += timestep * (Ekin / ttt - 1.0) * inv_tau_eta;

  ttt  = timestep * 2.0 * volume * inv_tau_xi * DIM / (nactive * temperature);

  xi_tmp   = xi_old.x + ttt * (stress_x - pressure_ext.x);
  xi_old.x = xi.x;
  xi.x     = xi_tmp;
  xi_tmp   = xi_old.y + ttt * (stress_y - pressure_ext.y);
  xi_old.y = xi.y;
  xi.y     = xi_tmp;
#ifndef TWOD
  xi_tmp   = xi_old.z + ttt * (stress_z - pressure_ext.z);
  xi_old.z = xi.z;
  xi.z     = xi_tmp;
#endif

  /* new box size (box is rectangular) */
  box_x.x *= box_size.x;  tbox_x.x /= box_size.x;
  box_y.y *= box_size.y;  tbox_y.y /= box_size.y;
#ifndef TWOD
  box_z.z *= box_size.z;  tbox_z.z /= box_size.z;
#endif  

  /* old box size relative to new one */
  box_size.x  = 1.0 / box_size.x;
  box_size.y  = 1.0 / box_size.y;
#ifndef TWOD
  box_size.z  = 1.0 / box_size.z;
#endif  

  /* check whether box has not changed too much */
  if  ((actual_shrink.x < limit_shrink.x) 
    || (actual_shrink.y < limit_shrink.y) 
#ifndef TWOD
    || (actual_shrink.z < limit_shrink.z) 
#endif
     ) {
     cells_too_small = 1;
  }
  if  ((actual_shrink.x < limit_shrink.x) || (actual_shrink.x > limit_growth.x)
    || (actual_shrink.y < limit_shrink.y) || (actual_shrink.y > limit_growth.y)
#ifndef TWOD
    || (actual_shrink.z < limit_shrink.z) || (actual_shrink.z > limit_growth.z)
#endif
     ) {
     revise_cell_division = 1;
  }
}

#else

void move_atoms_npt_axial(void) 
{
  if (myid==0)
  error("the chosen ensemble NPT_AXIAL is not supported by this binary");
}

#endif


/*****************************************************************************
*
*  Monte Carlo "integrator" 
*
*****************************************************************************/

#ifdef MC

void move_atoms_mc(void)
{
   one_mc_step();
}

#else

void move_atoms_mc(void)
{
  if (myid==0)
  error("the chosen ensemble MC is not supported by this binary");
}

#endif


/*****************************************************************************
*
*  NVE Integrator with stadium damping and fixed borders 
*  for fracture studies
*
*****************************************************************************/

#ifdef FRAC

void move_atoms_frac(void)

{
  int k;
  vektor stad, c_halbe;
  real tmp;
  static int count = 0;
  tot_kin_energy = 0;

  stad.x = 2 / stadium.x;
  stad.y = 2 / stadium.y;

  c_halbe.x = box_x.x / 2.0;
  c_halbe.y = box_y.y / 2.0;

  /* loop over all atoms */
#ifdef _OPENMP
#pragma omp parallel for reduction(+:tot_kin_energy)
#endif
  for (k=0; k<ncells; ++k) {

    int i;
    cell *p;
    real kin_energie_1, kin_energie_2, gamma, tmp1, tmp2;
    p = cell_array + CELLS(k);

    for (i=0; i<p->n; ++i) {

      /* do not move particles with negative numbers */
      if (NUMMER(p,i)>=0) {

        kin_energie_1 =  SPRODN(p->impuls,i,p->impuls,i);

        /* Calculate stadium function */
        tmp1 = SQR((p->ort X(i) - c_halbe.x) / 2*stadium.x);
        tmp2 = SQR((p->ort Y(i) - c_halbe.y) / 2*stadium.y);
        /* versteht das jemand?? */
        gamma = gamma_bar * (SQR(tmp1 + tmp2) - 2 * (tmp1 - tmp2) + 1);

        if (gamma_cut <= gamma) gamma = gamma_cut;
        if ((SQR((p->ort X(i) - c_halbe.x)  * stad.x)
            + SQR((p->ort Y(i) - c_halbe.y) * stad.y)) < 1.0) gamma=0;
                  
        /* new momenta */
        tmp1 = (1 - (1/(2 * MASSE(p,i))) * gamma * timestep);
        tmp2 = (1 + (1/(2 * MASSE(p,i))) * gamma * timestep);
        p->impuls X(i) = (p->kraft X(i)*timestep + p->impuls X(i)*tmp1)/tmp2;
        p->impuls Y(i) = (p->kraft Y(i)*timestep + p->impuls Y(i)*tmp1)/tmp2;
#ifndef TWOD
        p->impuls Z(i) = (p->kraft Z(i)*timestep + p->impuls Z(i)*tmp1)/tmp2;
#endif

        kin_energie_2 =  SPRODN(p->impuls,i,p->impuls,i);

        /* sum up kinetic energy */ 
        tot_kin_energy += (kin_energie_1 + kin_energie_2) / (4*MASSE(p,i));

        /* new positions */
        tmp1 = timestep / MASSE(p,i);
        p->ort X(i) += tmp1 * p->impuls X(i);
        p->ort Y(i) += tmp1 * p->impuls Y(i);
#ifndef TWOD
        p->ort Z(i) += tmp1 * p->impuls Z(i);
#endif

#ifdef STRESS_TENS
        p->presstens        X(i) += p->impuls X(i) * p->impuls X(i)/MASSE(p,i);
        p->presstens        Y(i) += p->impuls Y(i) * p->impuls Y(i)/MASSE(p,i);
#ifdef TWOD
        p->presstens_offdia[i]   += p->impuls X(i) * p->impuls Y(i)/MASSE(p,i);
#else
        p->presstens Z(i)        += p->impuls Z(i) * p->impuls Z(i)/MASSE(p,i);
        p->presstens_offdia X(i) += p->impuls Y(i) * p->impuls Z(i)/MASSE(p,i);
        p->presstens_offdia Y(i) += p->impuls Z(i) * p->impuls X(i)/MASSE(p,i);
        p->presstens_offdia Z(i) += p->impuls X(i) * p->impuls Y(i)/MASSE(p,i);
#endif
#endif
      }
    }
  }

#ifdef MPI
  /* Add kinetic energy for all CPUs */
  MPI_Allreduce( &tot_kin_energy, &tmp, 1, REAL, MPI_SUM, cpugrid);
  tot_kin_energy = tmp;
#endif

}

#else

void move_atoms_frac(void) 
{
  if (myid==0)
  error("the chosen ensemble FRAC is not supported by this binary");
}

#endif

#ifdef STM

/*****************************************************************************
*
*  NVT Integrator with Stadium 
*
*****************************************************************************/

void move_atoms_stm(void)

{
  int k;
  real kin_energie_1 = 0.0, kin_energie_2 = 0.0;
  real tmpvec1[2], tmpvec2[2], ttt;
   
  /* loop over all atoms */
#ifdef _OPENMP
#pragma omp parallel for reduction(+:kin_energie_1,kin_energie_2)
#endif
  for (k=0; k<ncells; ++k) {

    int i;
    cell *p;
    real reibung, eins_d_reib;
    real tmp, tmp1, tmp2;
    vektor d;
    int sort=0;

    p = cell_array + CELLS(k);

    for (i=0; i<p->n; ++i) {

        /* Check if outside or inside the ellipse: */	
        tmp = SQR((p->ort X(i)-center.x)/stadium.x) +
              SQR((p->ort Y(i)-center.y)/stadium.y) - 1;
        if (tmp <= 0) {
          /* We are inside the ellipse: */
          reibung = 1.0;
          eins_d_reib = 1.0;
        } else {
          reibung     =      1 - eta * inv_tau_eta * timestep / 2.0;
          eins_d_reib = 1 / (1 + eta * inv_tau_eta * timestep / 2.0);
        }

        /* twice the old kinetic energy */
        kin_energie_1 +=  SPRODN(p->impuls,i,p->impuls,i) / MASSE(p,i);

        /* new momenta */
	sort = VSORTE(p,i);
	p->impuls X(i) = (p->impuls X(i)*reibung + timestep * p->kraft X(i))
                          * eins_d_reib * (restrictions + sort)->x;
        p->impuls Y(i) = (p->impuls Y(i)*reibung + timestep * p->kraft Y(i))
                          * eins_d_reib * (restrictions + sort)->y;

        /* twice the new kinetic energy */ 
        kin_energie_2 +=  SPRODN(p->impuls,i,p->impuls,i) / MASSE(p,i);

        /* new positions */
        tmp = timestep * MASSE(p,i);
        p->ort X(i) += tmp * p->impuls X(i);
        p->ort Y(i) += tmp * p->impuls Y(i);
    }
  }
  
  tot_kin_energy = (kin_energie_1 + kin_energie_2) / 4.0;

#ifdef MPI
  /* add up results from all CPUs */
  tmpvec1[0] = tot_kin_energy;
  tmpvec1[1] = kin_energie_2;

  MPI_Allreduce( tmpvec1, tmpvec2, 2, REAL, MPI_SUM, cpugrid);

  tot_kin_energy = tmpvec2[0];
  kin_energie_2  = tmpvec2[1];
#endif

  /* Zeitentwicklung der Parameter */
  ttt  = nactive * temperature;
  eta += timestep * (kin_energie_2 / ttt - 1.0) * inv_tau_eta;

}

#else

void move_atoms_stm(void) 
{
  if (myid==0)
  error("the chosen ensemble STM is not supported by this binary");
}

#endif

/******************************************************************************
*
*  NVX Integrator for heat conductivity
* 
******************************************************************************/

#ifdef NVX

void move_atoms_nvx(void)

{
  int  k;
  real Ekin_1, Ekin_2;
  real Ekin_left = 0.0, Ekin_right = 0.0;
  int  natoms_left = 0, natoms_right = 0;
  real px, vol, real_tmp;
  int  num, nhalf, int_tmp;  
  real scale, rescale, Rescale;
  vektor tot_impuls_left, tot_impuls_right, vectmp;
  real inv_mass_left=0.0, inv_mass_right=0.0;
 
  tot_kin_energy = 0.0;
  tot_impuls_left.x  = 0.0;
  tot_impuls_right.x = 0.0;
  tot_impuls_left.y  = 0.0;
  tot_impuls_right.y = 0.0;
#ifndef TWOD
  tot_impuls_left.z  = 0.0;
  tot_impuls_right.z = 0.0;
#endif

  nhalf = tran_nlayers / 2;
  scale = tran_nlayers / box_x.x;

  /* loop over all atoms */
  for (k=0; k<ncells; ++k) {

    int i;
    cell *p;
    p = cell_array + CELLS(k);

    for (i=0; i<p->n; ++i) {

      Ekin_1 = SPRODN(p->impuls,i,p->impuls,i) / MASSE(p,i); 
      px = p->impuls X(i);

      /* new momenta */
      p->impuls X(i) += timestep * p->kraft X(i); 
      p->impuls Y(i) += timestep * p->kraft Y(i); 
#ifndef TWOD
      p->impuls Z(i) += timestep * p->kraft Z(i); 
#endif

      /* twice the new kinetic energy */ 
      Ekin_2 = SPRODN(p->impuls,i,p->impuls,i) / MASSE(p,i); 
      px = (px + p->impuls X(i)) / 2.0;

      /* new positions */
      p->ort X(i) += timestep * p->impuls X(i) / MASSE(p,i);
      p->ort Y(i) += timestep * p->impuls Y(i) / MASSE(p,i);
#ifndef TWOD
      p->ort Z(i) += timestep * p->impuls Z(i) / MASSE(p,i);
#endif

      tot_kin_energy += (Ekin_1 + Ekin_2) / 4.0;

      /* which layer */
      num = scale * p->ort X(i);
      if (num < 0)             num = 0;
      if (num >= tran_nlayers) num = tran_nlayers-1;

      /* temperature control and heat conductivity */
      if (num == 0) {
        Ekin_left += Ekin_2;
        inv_mass_left     += 1/(MASSE(p,i));
        tot_impuls_left.x += p->impuls X(i);
        tot_impuls_left.y += p->impuls Y(i);
#ifndef TWOD
        tot_impuls_left.z += p->impuls Z(i);
#endif
        natoms_left++;
      } else if  (num == nhalf) {
        Ekin_right += Ekin_2;
        inv_mass_right     += 1/(MASSE(p,i));
        tot_impuls_right.x += p->impuls X(i);
        tot_impuls_right.y += p->impuls Y(i);
#ifndef TWOD
        tot_impuls_right.z += p->impuls Z(i);
#endif
        natoms_right++;
      } else if (num < nhalf) {
        heat_cond += (p->heatcond[i] + (Ekin_1 + Ekin_2)/2.0 )
                                  * px / MASSE(p,i);
      } else {
        heat_cond -= (p->heatcond[i] + (Ekin_1 + Ekin_2)/2.0 )
                                  * px / MASSE(p,i);
      }
    }
  }

#ifdef MPI
  /* Add up results from all cpus */
  MPI_Allreduce( &tot_kin_energy, &real_tmp, 1, REAL, MPI_SUM, cpugrid);
  tot_kin_energy                 = real_tmp;
  MPI_Allreduce( &Ekin_left,      &real_tmp, 1, REAL, MPI_SUM, cpugrid);
  Ekin_left                      = real_tmp;
  MPI_Allreduce( &Ekin_right,     &real_tmp, 1, REAL, MPI_SUM, cpugrid);
  Ekin_right                     = real_tmp;
  MPI_Allreduce( &inv_mass_left,  &real_tmp, 1, REAL, MPI_SUM, cpugrid);
  inv_mass_left                  = real_tmp;
  MPI_Allreduce( &inv_mass_right, &real_tmp, 1, REAL, MPI_SUM, cpugrid);
  inv_mass_right                 = real_tmp;
  MPI_Allreduce( &tot_impuls_left,&vectmp, DIM, REAL, MPI_SUM, cpugrid);
  tot_impuls_left                = vectmp;
  MPI_Allreduce(&tot_impuls_right,&vectmp, DIM, REAL, MPI_SUM, cpugrid);
  tot_impuls_right               = vectmp;
  MPI_Allreduce( &natoms_left,    &int_tmp,  1, MPI_INT,  MPI_SUM, cpugrid);
  natoms_left                    = int_tmp;
  MPI_Allreduce( &natoms_right,   &int_tmp,  1, MPI_INT,  MPI_SUM, cpugrid);
  natoms_right                   = int_tmp;
#endif

  inv_mass_left      /= 2.0;
  inv_mass_right     /= 2.0;
  tot_impuls_left.x  /= natoms_left;
  tot_impuls_right.x /= natoms_right;
  tot_impuls_left.y  /= natoms_left;
  tot_impuls_right.y /= natoms_right;
#ifndef TWOD
  tot_impuls_left.z  /= natoms_left;
  tot_impuls_right.z /= natoms_right;
#endif

  /* rescale factors for momenta */
  real_tmp = Ekin_left
             - inv_mass_left * SPROD(tot_impuls_left,tot_impuls_left);
  rescale = sqrt( DIM * tran_Tleft * natoms_left / real_tmp  );
  real_tmp = Ekin_right
             - inv_mass_right * SPROD(tot_impuls_right,tot_impuls_right);
  Rescale = sqrt( DIM * tran_Tright * natoms_right / real_tmp  );

  for (k=0; k<ncells; ++k) {

    int i;
    cell *p;
    p = cell_array + CELLS(k);

    for (i=0; i<p->n; ++i) {
	    
      /* which layer? */
      num = scale * p->ort X(i);
      if (num < 0)             num = 0;
      if (num >= tran_nlayers) num = tran_nlayers-1;

      /* rescale momenta */
      if (num == 0) {
        p->impuls X(i) = (p->impuls X(i)-tot_impuls_left.x)*rescale;
        p->impuls Y(i) = (p->impuls Y(i)-tot_impuls_left.y)*rescale;
#ifndef TWOD
        p->impuls Z(i) = (p->impuls Z(i)-tot_impuls_left.z)*rescale;
#endif
      } else if (num == nhalf) {
        p->impuls X(i) = (p->impuls X(i)-tot_impuls_right.x)*Rescale;
        p->impuls Y(i) = (p->impuls Y(i)-tot_impuls_right.y)*Rescale;
#ifndef TWOD
        p->impuls Z(i) = (p->impuls Z(i)-tot_impuls_right.z)*Rescale;
#endif
      }

#ifdef STRESS_TENS
      p->presstens        X(i) += p->impuls X(i) * p->impuls X(i) / MASSE(p,i);
      p->presstens        Y(i) += p->impuls Y(i) * p->impuls Y(i) / MASSE(p,i);
#ifdef TWOD
      p->presstens_offdia[i]   += p->impuls X(i) * p->impuls Y(i) / MASSE(p,i);
#else
      p->presstens Z(i)        += p->impuls Z(i) * p->impuls Z(i) / MASSE(p,i);
      p->presstens_offdia X(i) += p->impuls Y(i) * p->impuls Z(i) / MASSE(p,i);
      p->presstens_offdia Y(i) += p->impuls Z(i) * p->impuls X(i) / MASSE(p,i);
      p->presstens_offdia Z(i) += p->impuls X(i) * p->impuls Y(i) / MASSE(p,i);
#endif
#endif
    }
  }
#ifdef RNEMD
  heat_transfer += tran_Tleft *natoms_left  * DIM/2 - Ekin_left/2;  /* hot  */ 
  heat_transfer -= tran_Tright*natoms_right * DIM/2 - Ekin_right/2; /* cold */
#endif
}

#else

void move_atoms_nvx(void) 
{
  if (myid==0) 
  error("the chosen ensemble NVX is not supported by this binary");
}

#endif

