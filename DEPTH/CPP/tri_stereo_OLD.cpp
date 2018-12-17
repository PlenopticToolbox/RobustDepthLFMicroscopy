#include <math.h>
#include <matrix.h>
#include <mex.h>
#include <iostream>
#include <cmath>

void map_index_to_coords(int map, int &c1, int &c2) {
    
// horizontal epipolar line
    if (map == 1)
    {
        c1 = 0;
        c2 = 2;
    } 
    // 60 degrees (left top, right bottom)
    else if (map == 2)
    {
        c1 = 1;
        c2 = 1;
    }
    // 120 degrees (left bottom, right top)
    else if (map == 3)
    {
        c1 = 1;
        c2 = -1;
    }
    
}

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[]) {
    //variables
    const mwSize *dimsL, *dimsR, *dimsC;
    int xL, yL, xR, yR, xC, yC;
    double * cost_volume_L, * cost_volume_R, * imgL, * imgR, *imgC;
    int i,j,k,h,w,d;
    int c1, c2, i_, j_;
  
    if (nrhs != 8)
    {
        mexErrMsgTxt("Eight inputs required. Left, Center and Right Image (BW). Dmin, Dmax, Window Size, Alpha and Map");
    }
    
    //figure out dimensions
    dimsL = mxGetDimensions(prhs[0]);
    dimsC = mxGetDimensions(prhs[1]);
    dimsR = mxGetDimensions(prhs[2]);
    yL = (int)dimsL[0]; xL = (int)dimsL[1];
    yC = (int)dimsC[0]; xC = (int)dimsC[1];
    yR = (int)dimsR[0]; xR = (int)dimsR[1];
    
    int dmin = *mxGetPr(prhs[3]);
    int dmax = *mxGetPr(prhs[4]);
    int numdisp = (dmax - dmin)+1;
    const mwSize cost_volume_dims[]={xL,yL,numdisp};
    plhs[0] = mxCreateNumericArray(3, cost_volume_dims, mxDOUBLE_CLASS, mxREAL);
    plhs[1] = mxCreateNumericArray(3, cost_volume_dims, mxDOUBLE_CLASS, mxREAL);
    imgL = mxGetPr(prhs[0]);
    imgC = mxGetPr(prhs[1]);
    imgR = mxGetPr(prhs[2]);
    cost_volume_L = mxGetPr(plhs[0]);
    cost_volume_R = mxGetPr(plhs[1]);
    int ws = *mxGetPr(prhs[5]);
    double alpha = *mxGetPr(prhs[6]);
    int hws = floor(ws/2);
    int dd = 0;
    int left_census[ws][ws];
    int central_census[ws][ws];
    int right_census[ws][ws];
    int map = *mxGetPr(prhs[7]);
    double sad_sumL = 0.0; double sad_sumR = 0.0;
    double TRUNC_AD = 50;
    double census_hdL = 0.0; double census_hdR = 0.0;
    double weightL = 0.0; double weightR = 0.0;
    const double THRESH_WEIGHT = 1.0;
    const double MAX_COST = 10.0;
    const double gamma = 2.5;
    double g1 = (ws*ws);
    double g2 = 25.0;
    double leftVal = 0.0; double rightVal = 0.0;
    int pad = std::max(std::abs(dmin), std::abs(dmax)); // we accept also negative disparities
    double weight_sumL = 0.0; double weight_sumR = 0.0;
    mexPrintf("dmin=%d, dmax=%d, ws=%d, hws=%d, alpha=%3.3f\n", dmin, dmax, ws, hws, alpha);
    for(i=pad+hws; i<xL-pad-hws; i++) //dmax+hws+1;i<s1-dmax-hws-1;i++) // 100;i<200;i++) // //(i=hws+1;i<s1-hws-1;i++)
    {
        for(j=pad+hws; j<yL-pad-hws; j++) //dmax+hws+1;j<s2-dmax-hws-1;j++) //  300;j<400;j++) //  (j=hws+1;j<s2-hws-1;j++)
        {       
            for(h=-hws;h<hws;h++)
            {
                for(w=-hws;w<hws;w++)
                {
                    if (imgC[i+h+(j+w)*xL] < imgC[i+(j)*xL])
                        central_census[h+hws][w+hws] = 0;
                    else
                        central_census[h+hws][w+hws] = 1;
                }
            } 
            
            // second round
            for (d=dmin; d<=dmax; d++)
            {
                if (d == 0)
                {
                    sad_sumL = (MAX_COST); // /= (ws*TRUNC_AD);
                    sad_sumR = (MAX_COST);
                    census_hdL = (MAX_COST);
                    census_hdR = (MAX_COST);
                }
                else
                {
                    map_index_to_coords(map, c1, c2);
                    i_ = round(i+c1*0.866*(d));
                    j_ = round(j+c2*0.5*(d));
                    sad_sumL = 0.0;
                    census_hdL = 0.0;
                    sad_sumR = 0.0;
                    census_hdR = 0.0;
                    leftVal = imgL[i+(j-d)*xL];
                    rightVal = imgR[i+(j+d)*xL];
                    for(h=-hws;h<hws;h++)
                    {
                        for(w=-hws;w<hws;w++)
                        {
                            // build census windows
                            if (imgR[i+h+(j+d+w)*xL] < imgR[i+(j+d)*xL])
                                right_census[h+hws][w+hws] = 0;
                            else
                                right_census[h+hws][w+hws] = 1;
                            if (imgL[i+h+(j-d+w)*xL] < imgL[i+(j-d)*xL])
                                left_census[h+hws][w+hws] = 0;
                            else
                                left_census[h+hws][w+hws] = 1;

                            //weights --> using adaptive support windows
                            if (std::abs(rightVal - imgR[i+h+(j+d+w)*xL]) < THRESH_WEIGHT)
                                weightR = 1.0;
                            else
                                weightR = exp(-(sqrt((h*h)+(w*w))/g1 + std::abs(rightVal - imgR[i+h+(j+w+d)*xL]) / g2));
                            if (std::abs(leftVal - imgL[i+h+(j-d+w)*xL]) < THRESH_WEIGHT)
                                weightL = 1.0;
                            else
                                weightL = exp(-(sqrt((h*h)+(w*w))/g1 + std::abs(imgL[i+h+(j+w-d)*xL] - leftVal) / g2));
                            //mexPrintf("weightL=%3.3f, weightR=%3.3f, h=%d, w=%d, LD=%3.3f, RD=%3.3f\n", weightL, weightR, h, w, std::abs(imgL[i+h+(j+w-d)*xL] - leftVal), std::abs(rightVal - imgR[i+h+(j+w+d)*xL]));
                            // add the weighted cost
                            census_hdL += weightL * std::abs(left_census[h+hws][w+hws] - central_census[h+hws][w+hws]);
                            census_hdR += weightR * std::abs(right_census[h+hws][w+hws] - central_census[h+hws][w+hws]);
                            sad_sumL += weightL * std::min(TRUNC_AD, std::abs(imgC[i+h+(j+w)*xL] - imgL[i+h+(j+w-d)*xL]));
                            sad_sumR += weightR * std::min(TRUNC_AD, std::abs(imgC[i+h+(j+w)*xL] - imgR[i+h+(j+w+d)*xL]));
                            weight_sumL += weightL;
                            weight_sumR += weightR;
                            //mexPrintf("LEFT: %3.3f - %3.3f = %3.3f\n", imgL[i+h+(j+w)*xL], imgL[i+h+(j+w-d)*xL], std::abs(imgL[i+h+(j+w)*xL] - imgL[i+h+(j+w-d)*xL]));
                            //mexPrintf("sad_sumL: %3.3f\n", sad_sumL);
                            //mexPrintf("censR=%d-%d=%d\n", right_census[h+hws][w+hws], central_census[h+hws][w+hws], std::abs(right_census[h+hws][w+hws] - central_census[h+hws][w+hws]));
                        }
                    }
                    //mexPrintf("before: sadL=%3.3f, sadR=%3.3f, censusL=%3.3f, censusR=%3.3f\n", sad_sumL, sad_sumR, census_hdL, census_hdR);
                    sad_sumL = std::min(sad_sumL/gamma, MAX_COST); // /= (ws*TRUNC_AD);
                    sad_sumR = std::min(sad_sumR/gamma, MAX_COST);
                    census_hdL = std::min(census_hdL, MAX_COST);
                    census_hdR = std::min(census_hdR, MAX_COST);
                    // d is the disparity, dd the index in the array (dd >= 0)
                }
                dd = d - dmin; // works for both positive and negative dmin;
                //mexPrintf("after: sadL=%3.3f, sadR=%3.3f, censusL=%3.3f, censusR=%3.3f\n", sad_sumL, sad_sumR, census_hdL, census_hdR);
                //mexPrintf("cv=%3.3f\n", sad_sumL*alpha+census_hdL*(1-alpha));
                cost_volume_L[i + j * xL + dd * (xL * yL)] = sad_sumL*alpha+census_hdL*(1-alpha);
                cost_volume_R[i + j * xL + dd * (xL * yL)] = sad_sumR*alpha+census_hdR*(1-alpha);
            }
        }
    }

    return;
}
            