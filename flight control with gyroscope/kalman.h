/*
 * =====================================================================================
 *
 *       Filename:  kalman.h
 *
 *    Description: ported from https://github.com/TKJElectronics/KalmanFilter 
 *
 *        Version:  1.0
 *        Created:  11/20/2014 03:10:00 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Prem swaroop k (), premswaroop@asu.edu
 *   Organization:  
 *
 * =====================================================================================
 */
/* Copyright (C) 2012 Kristian Lauszus, TKJ Electronics. All rights reserved.
 
 This software may be distributed and modified under the terms of the GNU
 General Public License version 2 (GPL2) as published by the Free Software
 Foundation and appearing in the file GPL2.TXT included in the packaging of
 this file. Please note that GPL2 Section 2[b] requires that all works based
 on this software must also be made publicly available under the terms of
 the GPL2 ("Copyleft").
 
 Contact information
 -------------------
 
 Kristian Lauszus, TKJ Electronics
 Web      :  http://www.tkjelectronics.com
 e-mail   :  kristianl@tkjelectronics.com
 */

#ifndef _Kalman_h
#define _Kalman_h

struct Kalman {
    /* Kalman filter variables */
    float Q_angle; // Process noise variance for the accelerometer
    float Q_bias; // Process noise variance for the gyro bias
    float R_measure; // Measurement noise variance - this is actually the variance of the measurement noise
    
    float angle; // The angle calculated by the Kalman filter - part of the 2x1 state matrix
    float bias; // The gyro bias calculated by the Kalman filter - part of the 2x1 state matrix
    float rate; // Unbiased rate calculated from the rate and the calculated bias - you have to call getAngle to update the rate
    
    float P[2][2]; // Error covariance matrix - This is a 2x2 matrix
    float K[2]; // Kalman gain - This is a 2x1 matrix
    float y; // Angle difference - 1x1 matrix
    float S; // Estimate error - 1x1 matrix
};
void Kalman_init(struct Kalman *x);
float getAngle(struct Kalman *x, float newAngle, float newRate, float dt); 
// The angle should be in degrees and the rate should be in degrees per second and the delta time in seconds
void setAngle(struct Kalman *x,float newAngle); // Used to set angle, this should be set as the starting angle
float getRate(struct Kalman *x); // Return the unbiased rate
/* These are used to tune the Kalman filter */
void setQangle(struct Kalman *x,float newQ_angle);
void setQbias(struct Kalman *x,float newQ_bias);
void setRmeasure(struct Kalman *x,float newR_measure);
#endif


