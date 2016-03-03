/*
 * =====================================================================================
 *
 *       Filename:  kalman.c
 *
 *    Description:  ported from https://github.com/TKJElectronics/KalmanFilter.
 *
 *        Version:  1.0
 *        Created:  11/20/2014 03:27:32 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Prem swaroop k (), premswaroop@asu.edu
 *   Organization:  
 *
 * =====================================================================================
 */
#include "kalman.h"

void Kalman_init(struct Kalman *x)
 {
        /* We will set the varibles like so, these can also be tuned by the user */
        x->Q_angle = 0.001;
        x->Q_bias = 0.003;
        x->R_measure = 0.03;
        
        x->bias = 0; // Reset bias
        x->P[0][0] = 0; // Since we assume tha the bias is 0 and we know the starting angle (use setAngle), the error covariance matrix is set like so - see: http://en.wikipedia.org/wiki/Kalman_filter#Example_application.2C_technical
        x->P[0][1] = 0;
        x->P[1][0] = 0;
        x->P[1][1] = 0;
    }
float getAngle(struct Kalman *x, float newAngle, float newRate, float dt) 
{
        // KasBot V2  -  Kalman filter module - http://www.x-firm.com/?page_id=145
        // Modified by Kristian Lauszus
        // See my blog post for more information: http://blog.tkjelectronics.dk/2012/09/a-practical-approach-to-kalman-filter-and-how-to-implement-it
                        
        // Discrete Kalman filter time update equations - Time Update ("Predict")
        // Update xhat - Project the state ahead
        float (*P)[2] = x->P; // Error covariance matrix - This is a 2x2 matrix
        float *K = x->K; // Kalman gain - This is a 2x1 matrix

        /* Step 1 */
        x->rate = newRate - x->bias;
        x->angle += dt * x->rate;
        
        // Update estimation error covariance - Project the error covariance ahead
        /* Step 2 */
        P[0][0] += dt * (dt*P[1][1] - P[0][1] - P[1][0] + x->Q_angle);
        P[0][1] -= dt * P[1][1];
        P[1][0] -= dt * P[1][1];
        P[1][1] += x->Q_bias * dt;
        
        // Discrete Kalman filter measurement update equations - Measurement Update ("Correct")
        // Calculate Kalman gain - Compute the Kalman gain
        /* Step 4 */
        x->S = P[0][0] + x->R_measure;
        /* Step 5 */
        K[0] = P[0][0] / x->S;
        K[1] = P[1][0] / x->S;
        
        // Calculate angle and bias - Update estimate with measurement zk (newAngle)
        /* Step 3 */
        x->y = newAngle - x->angle;
        /* Step 6 */
        x->angle += K[0] * x->y;
        x->bias += K[1] * x->y;
        
        // Calculate estimation error covariance - Update the error covariance
        /* Step 7 */
        P[0][0] -= K[0] * P[0][0];
        P[0][1] -= K[0] * P[0][1];
        P[1][0] -= K[1] * P[0][0];
        P[1][1] -= K[1] * P[0][1];
        
        return x->angle;
    };
    // The angle should be in degrees and the rate should be in degrees per second and the delta time in seconds
    void setAngle(struct Kalman *x,float newAngle) { x->angle = newAngle; } // Used to set angle, this should be set as the starting angle
    float getRate(struct Kalman *x) { return x->rate; } // Return the unbiased rate
    
    /* These are used to tune the Kalman filter */
    void setQangle(struct Kalman *x,float newQ_angle) { x->Q_angle = newQ_angle; }
    void setQbias(struct Kalman *x,float newQ_bias) { x->Q_bias = newQ_bias; }
    void setRmeasure(struct Kalman *x,float newR_measure) { x->R_measure = newR_measure; }



