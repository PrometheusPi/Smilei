#ifndef COLLISIONS_H
#define COLLISIONS_H

#include <vector>
#include <cmath>

#include "Tools.h"
#include "H5.h"
#include "CollisionalIonization.h"

class Patch;
class Params;
class Species;
class VectorPatch;

class Collisions
{

public:
    //! Constructor for Collisions between two species
    Collisions( Params &params, unsigned int n_collisions, std::vector<unsigned int>,
                std::vector<unsigned int>, double coulomb_log, bool intra_collisions,
                int debug_every, int Z, int ionization_electrons, Particles * ionization_particles,
                int nDim, std::string );
    //! Cloning Constructor
    Collisions( Collisions *, int );
    //! destructor
    virtual ~Collisions();
    
    //! Method to calculate the Debye length in each cluster
    static void calculate_debye_length( Params &, Patch * );
    
    //! is true if any of the collisions objects need automatically-computed coulomb log
    static bool debye_length_required;
    
    //! Method called in the main smilei loop to apply collisions at each timestep
    virtual void collide( Params &, Patch *, int, std::vector<Diagnostic *> & );
    
    //! Outputs the debug info if requested
    static void debug( Params &params, int itime, unsigned int icoll, VectorPatch &vecPatches );
    
    //! CollisionalIonization object, created if ionization required
    CollisionalIonization *Ionization;
    
protected:

    //! Identification number of the Collisions object
    int n_collisions_;
    
    //! Group of the species numbers that are associated for Collisions.
    std::vector<unsigned int> species_group1_, species_group2_;
    
    //! Coulomb logarithm (zero or negative means automatic)
    double coulomb_log_;
    
    //! True if collisions inside a group of species, False if collisions between different groups of species
    bool intra_collisions_;
    
    //! Number of timesteps between each dump of collisions debugging
    int debug_every_;
    
    //! Species atomic number, in case of ionization
    int atomic_number;
    
    //! Hdf5 file name
    std::string filename_;
    
    //! Temporary variables for the debugging file
    double smean_, logLmean_;//, ncol;//, temperature
    
    const double twoPi = 2. * 3.14159265358979323846;
    double coeff1_, coeff2_;
    
    // Collide one particle with another
    // See equations in http://dx.doi.org/10.1063/1.4742167
    inline double one_collision(
        Particles *p1,
        unsigned int i1,
        double m1,
        Particles *p2,
        unsigned int i2,
        double m12,
        double coeff1,
        double coeff2,
        double coeff3,
        double coeff4,
        double n123,
        double n223,
        double debye2,
        double &logL,
        double U1,
        double U2,
        double phi
    )
    {
        double qqm, qqm2, gamma1, gamma2, gamma12, gamma12_inv,
               COM_vx, COM_vy, COM_vz, COM_vsquare, COM_gamma,
               term1, term2, term3, term4, term5, term6,
               vcv1, vcv2, px_COM, py_COM, pz_COM, p2_COM, p_COM, gamma1_COM, gamma2_COM,
               bmin, s, vrel, smax,
               cosX, sinX, sinXcosPhi, sinXsinPhi, p_perp, inv_p_perp,
               newpx_COM, newpy_COM, newpz_COM, vcp;
               
        // Calculate stuff
        qqm  = p1->charge( i1 ) * p2->charge( i2 ) / m1;
        qqm2 = qqm * qqm;
        
        // Get momenta and calculate gammas
        gamma1 = sqrt( 1. + p1->momentum( 0, i1 )*p1->momentum( 0, i1 ) + p1->momentum( 1, i1 )*p1->momentum( 1, i1 ) + p1->momentum( 2, i1 )*p1->momentum( 2, i1 ) );
        gamma2 = sqrt( 1. + p2->momentum( 0, i2 )*p2->momentum( 0, i2 ) + p2->momentum( 1, i2 )*p2->momentum( 1, i2 ) + p2->momentum( 2, i2 )*p2->momentum( 2, i2 ) );
        gamma12 = m12 * gamma1 + gamma2;
        gamma12_inv = 1./gamma12;
        
        // Calculate the center-of-mass (COM) frame
        // Quantities starting with "COM" are those of the COM itself, expressed in the lab frame.
        // They are NOT quantities relative to the COM.
        COM_vx = ( m12 * ( p1->momentum( 0, i1 ) ) + p2->momentum( 0, i2 ) ) * gamma12_inv;
        COM_vy = ( m12 * ( p1->momentum( 1, i1 ) ) + p2->momentum( 1, i2 ) ) * gamma12_inv;
        COM_vz = ( m12 * ( p1->momentum( 2, i1 ) ) + p2->momentum( 2, i2 ) ) * gamma12_inv;
        COM_vsquare = COM_vx*COM_vx + COM_vy*COM_vy + COM_vz*COM_vz;
        
        // Change the momentum to the COM frame (we work only on particle 1)
        // Quantities ending with "COM" are quantities of the particle expressed in the COM frame.
        if( COM_vsquare != 0. ) {
            COM_gamma = 1./sqrt( 1.-COM_vsquare );
            term1 = ( COM_gamma - 1. ) / COM_vsquare;
            vcv1  = ( COM_vx*( p1->momentum( 0, i1 ) ) + COM_vy*( p1->momentum( 1, i1 ) ) + COM_vz*( p1->momentum( 2, i1 ) ) )/gamma1;
            vcv2  = ( COM_vx*( p2->momentum( 0, i2 ) ) + COM_vy*( p2->momentum( 1, i2 ) ) + COM_vz*( p2->momentum( 2, i2 ) ) )/gamma2;
            term2 = ( term1*vcv1 - COM_gamma ) * gamma1;
            px_COM = ( p1->momentum( 0, i1 ) ) + term2*COM_vx;
            py_COM = ( p1->momentum( 1, i1 ) ) + term2*COM_vy;
            pz_COM = ( p1->momentum( 2, i1 ) ) + term2*COM_vz;
            gamma1_COM = ( 1.-vcv1 )*COM_gamma*gamma1;
            gamma2_COM = ( 1.-vcv2 )*COM_gamma*gamma2;
        } else {
            COM_gamma = 1.;
            term1 = 0.5;
            term2 = gamma1;
            px_COM = ( p1->momentum( 0, i1 ) );
            py_COM = ( p1->momentum( 1, i1 ) );
            pz_COM = ( p1->momentum( 2, i1 ) );
            gamma1_COM = gamma1;
            gamma2_COM = gamma2;
        }
        p2_COM = px_COM*px_COM + py_COM*py_COM + pz_COM*pz_COM;
        p_COM  = sqrt( p2_COM );
        
        // Calculate some intermediate quantities
        term3 = COM_gamma * gamma12_inv;
        term4 = gamma1_COM * gamma2_COM;
        term5 = term4/p2_COM + m12;
        
        // Calculate coulomb log if necessary
        if( logL <= 0. ) { // if auto-calculation requested
            bmin = std::max( coeff1/m1/p_COM, std::abs( coeff2*qqm*term3*term5 ) ); // min impact parameter
            logL = 0.5*log( 1.+debye2/( bmin*bmin ) );
            if( logL < 2. ) {
                logL = 2.;
            }
        }
        
        // Calculate the collision parameter s12 (similar to number of real collisions)
        s = coeff3 * logL * qqm2 * term3 * p_COM * term5*term5 / ( gamma1*gamma2 );
        
        // Low-temperature correction
        vrel = p_COM/term3/term4; // relative velocity
        smax = coeff4 * ( m12+1. ) * vrel / std::max( m12*n123, n223 );
        if( s>smax ) {
            s = smax;
        }
        
        // Pick the deflection angles
        // Technique given by Nanbu in http://dx.doi.org/10.1103/PhysRevE.55.4642
        //   to pick randomly the deflection angle cosine, in the center-of-mass frame.
        // Technique slightly modified in http://dx.doi.org/10.1063/1.4742167
        if( s < 0.1 ) {
            if( U1<0.0001 ) {
                U1=0.0001;    // ensures cos_chi > 0
            }
            cosX = 1. + s*log( U1 );
        } else if( s < 3. ) {
            // the polynomial has been modified from the article in order to have a better form
            double invA = 0.00569578 +( 0.95602 + ( -0.508139 + ( 0.479139 + ( -0.12789 + 0.0238957*s )*s )*s )*s )*s;
            double A = 1./invA;
            cosX = invA  * log( exp( -A ) + 2.*U1*sinh( A ) );
        } else if( s < 6. ) {
            double A = 3.*exp( -s );
            cosX = ( 1./A ) * log( exp( -A ) + 2.*U1*sinh( A ) );
        } else {
            cosX = 2.*U1 - 1.;
        }
        sinX = sqrt( 1. - cosX*cosX );
        
        // Calculate combination of angles
        sinXcosPhi = sinX*cos( phi );
        sinXsinPhi = sinX*sin( phi );
        
        // Apply the deflection
        p_perp = sqrt( px_COM*px_COM + py_COM*py_COM );
        if( p_perp > 1.e-10*p_COM ) { // make sure p_perp is not too small
            inv_p_perp = 1./p_perp;
            newpx_COM = ( px_COM * pz_COM * sinXcosPhi - py_COM * p_COM * sinXsinPhi ) * inv_p_perp + px_COM * cosX;
            newpy_COM = ( py_COM * pz_COM * sinXcosPhi + px_COM * p_COM * sinXsinPhi ) * inv_p_perp + py_COM * cosX;
            newpz_COM = -p_perp * sinXcosPhi  +  pz_COM * cosX;
        } else { // if p_perp is too small, we use the limit px->0, py=0
            newpx_COM = p_COM * sinXcosPhi;
            newpy_COM = p_COM * sinXsinPhi;
            newpz_COM = p_COM * cosX;
        }
        
        // Go back to the lab frame and store the results in the particle array
        vcp = COM_vx * newpx_COM + COM_vy * newpy_COM + COM_vz * newpz_COM;
        if( U2 < p2->weight( i2 )/p1->weight( i1 ) ) { // deflect particle 1 only with some probability
            term6 = term1*vcp + gamma1_COM * COM_gamma;
            p1->momentum( 0, i1 ) = newpx_COM + COM_vx * term6;
            p1->momentum( 1, i1 ) = newpy_COM + COM_vy * term6;
            p1->momentum( 2, i1 ) = newpz_COM + COM_vz * term6;
        }
        if( U2 < p1->weight( i1 )/p2->weight( i2 ) ) { // deflect particle 2 only with some probability
            term6 = -m12 * term1*vcp + gamma2_COM * COM_gamma;
            p2->momentum( 0, i2 ) = -m12 * newpx_COM + COM_vx * term6;
            p2->momentum( 1, i2 ) = -m12 * newpy_COM + COM_vy * term6;
            p2->momentum( 2, i2 ) = -m12 * newpz_COM + COM_vz * term6;
        }
        
        return s;
    }
};


#endif
