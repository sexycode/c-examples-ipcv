// Example : tree based recognition of largest closed contour in
// an image / video / camera
// usage: prog {<image_name> | <video_name>}

// Author : Toby Breckon, toby.breckon@cranfield.ac.uk

// Copyright (c) 2008 School of Engineering, Cranfield University
// License : LGPL - http://www.gnu.org/licenses/lgpl.html

#include "cv.h"       // open cv general include file
#include "highgui.h"  // open cv GUI include file

#include <stdio.h>
#include <algorithm> // contains max() function (amongst others)
using namespace cv; // use c++ namespace so the timing stuff works consistently

/******************************************************************************/
// setup the camera index properly based on OS platform

// 0 in linux gives first camera for v4l
//-1 in windows gives first device or user dialog selection

#ifdef linux
	#define CAMERA_INDEX 0
#else
	#define CAMERA_INDEX -1
#endif

/******************************************************************************/

// find the largest contour (by area) from a sequence of contours and return a
// pointer to that item in the sequence

CvSeq* findLargestContour(CvSeq* contours){

  CvSeq* current_contour = contours;
  double largestArea = 0;
  CvSeq* largest_contour = NULL;
  double area;

  // check we at least have some contours

  if (contours == NULL){return NULL;}

  // for each contour compare it to current largest area on
  // record and remember the contour with the largest area
  // (note the use of fabs() for the cvContourArea() function)

  while (current_contour != NULL){

	  area = fabs(cvContourArea(current_contour));

	  if(area > largestArea){
		  largestArea = area;
		  largest_contour = current_contour;
	  }

	  current_contour = current_contour->h_next;
  }

  // return pointer to largest

  return largest_contour;

}

/******************************************************************************/

int main( int argc, char** argv )
{

  IplImage* img;  			 		// input image object
  IplImage* grayImg = NULL;  		// tmp image object
  IplImage* thresholdedImg = NULL;  // thresholded image object
  IplImage* closeImage  = NULL;     // morphed image object
  IplImage* output = NULL; 		    // output image object

  int windowSize = 26; // starting threshold value
  int constant = 16; // starting constant value
  CvCapture* capture = NULL; // capture object

  char const * windowName1 = "OPENCV: adaptive image thresholding"; // window name
  char const * windowName2 = "OPENCV: Main Contour Image"; // window name
  char outputString[255]; // string for text output in a window

  bool keepProcessing = true;	// loop control flag
  char key;						// user input
  int  EVENT_LOOP_DELAY = 40;	// delay for GUI window
                                // 40 ms equates to 1000ms/25fps = 40ms per frame

  // initialise some contour finding stuff

  CvMemStorage* storage = cvCreateMemStorage(0);
  CvSeq* contours = NULL;
  CvSeq* largest_contour = NULL;

  IplConvKernel* structuringElement =
  	cvCreateStructuringElementEx(3, 3, 1, 1, CV_SHAPE_RECT, NULL);

  int iterations = 5;					 // closing iterations to apply

  // intialise recognition trees

  CvContourTree* model_tree = NULL;
  CvContourTree* data_tree = NULL;
  CvMemStorage* model_tree_storage = cvCreateMemStorage(0);
  CvMemStorage* data_tree_storage = cvCreateMemStorage(0);

  bool modelBuilding = true; 	// start by building contour model

  // if command line arguments are provided try to read image/video_name
  // otherwise default to capture from attached H/W camera

    if(
	  ( argc == 2 && (img = cvLoadImage( argv[1], 1)) != 0 ) ||
	  ( argc == 2 && (capture = cvCreateFileCapture( argv[1] )) != 0 ) ||
	  ( argc != 2 && (capture = cvCreateCameraCapture( 0 )) != 0 )
	  )
    {
      // create window objects

      cvNamedWindow(windowName1, 0 );
      cvNamedWindow(windowName2, 0 );

	  // add adjustable trackbar for threshold parameter

      cvCreateTrackbar("Neighbourhood (N)", windowName1, &windowSize, 255, NULL);
	  cvCreateTrackbar("Constant (C)", windowName1, &constant, 50, NULL);
	  cvCreateTrackbar("Closing", windowName1, &iterations, 25, NULL);

	  // set up font structure

	  CvFont font;
	  cvInitFont(&font, CV_FONT_HERSHEY_PLAIN, 1, 1 , 0, 2, 8 );

	  // if capture object in use (i.e. video/camera)
	  // get initial image from capture object

	  if (capture) {

		  // cvQueryFrame just a combination of cvGrabFrame
		  // and cvRetrieveFrame in one call.

		  img = cvQueryFrame(capture);
		  if(!img){
			if (argc == 2){
				printf("End of video file reached\n");
			} else {
				printf("ERROR: cannot get next fram from camera\n");
			}
			exit(0);
		  }

	  }

	  // create output images

	  thresholdedImg = cvCreateImage(cvSize(img->width,img->height),
						   img->depth, 1);
	  thresholdedImg->origin = img->origin;
	  grayImg = cvCreateImage(cvSize(img->width,img->height),
				img->depth, 1);
	  grayImg->origin = img->origin;

	  output = cvCloneImage(img);
	  closeImage = cvCloneImage(grayImg);

	  // start contour main processing loop

	  while (keepProcessing) {

          int64 timeStart = getTickCount(); // get time at start of loop

		  // if capture object in use (i.e. video/camera)
		  // get image from capture object

		  if (capture) {

			  // cvQueryFrame s just a combination of cvGrabFrame
			  // and cvRetrieveFrame in one call.

			  img = cvQueryFrame(capture);
			  if(!img){
				if (argc == 2){
					printf("End of video file reached\n");
				} else {
					printf("ERROR: cannot get next fram from camera\n");
				}
				exit(0);
			  }

		  }

		  // if input is not already grayscale, convert to grayscale

		  if (img->nChannels > 1){
			 cvCvtColor(img, grayImg, CV_BGR2GRAY);
	      } else {
			grayImg = img;
		  }

		  // check that the window size is always odd and > 3

		  if ((windowSize > 3) && (fmod((double) windowSize, 2) == 0)) {
				windowSize++;
		  } else if (windowSize < 3) {
			  windowSize = 3;
		  }

		  // threshold the image

		  cvAdaptiveThreshold(grayImg, thresholdedImg, 255,
		  						CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY,
		  						windowSize, constant);

		  // morphological closing

		   if (iterations < 1) {iterations++;}

		   cvNot(thresholdedImg, thresholdedImg);
		   cvMorphologyEx(thresholdedImg, closeImage, NULL, structuringElement,
		  											CV_MOP_CLOSE, iterations);

		  // display image in window

		  cvShowImage( windowName1, closeImage );

		  // find the contours

		  cvFindContours( closeImage, storage,
		  		&contours, sizeof(CvContour), CV_RETR_LIST,
		   		CV_CHAIN_APPROX_SIMPLE );

		  //draw first main contour

		  cvCopy(img, output);

		  largest_contour = findLargestContour(contours);

		  if (largest_contour != NULL)
		  {
		  	cvDrawContours( output, largest_contour,
		  				CV_RGB( 0, 0, 255 ), CV_RGB( 0, 0, 255 ),
		  				0, 2, 8, cvPoint(0,0) );
		  }

		  // build tree

		  if (largest_contour != NULL)
		 {
			// if the contour appears to be the wrong way around then flip it

			if (cvContourArea(largest_contour) < 0)
		  	{
			 	cvSeqInvert(largest_contour);
		  	}

			// then build the tree

			if (modelBuilding) {
				cvClearMemStorage(model_tree_storage);
				model_tree = cvCreateContourTree(largest_contour, model_tree_storage, 0);
			} else {
				cvClearMemStorage(data_tree_storage);
				data_tree = cvCreateContourTree(largest_contour, model_tree_storage, 0);
			}
		  }

		  // if we have a model built then do recognition

		  if (modelBuilding){
		  	cvPutText(output, "MODEL BUILDING",
			  		cvPoint(10,output->height - 5), &font, CV_RGB(0, 255,0));
		  } else {

			sprintf(outputString, "RECOGNITION: match score = %.2f",
			  100 * cvMatchContourTrees(model_tree, data_tree, CV_CONTOUR_TREES_MATCH_I1,
			  0.0001));

			cvPutText(output,outputString,
			  		cvPoint(10,output->height - 5), &font, CV_RGB(0, 255,0));
		  }

		  // clear detected contours

		  if (contours != NULL){
				cvClearSeq(contours);
		  }

		  // display image in window

		  cvShowImage( windowName2, output );

		  // start event processing loop (very important,in fact essential for GUI)
	      // 40 ms roughly equates to 1000ms/25fps = 40ms per frame

		  // here we take account of processing time for the loop by subtracting the time
          // taken in ms. from this (1000ms/25fps = 40ms per frame) value whilst ensuring
          // we get a +ve wait time


          key = cvWaitKey((int) std::max(2.0, EVENT_LOOP_DELAY -
                        (((getTickCount() - timeStart) / getTickFrequency()) * 1000)));

		  if (key == 'x'){

	   		// if user presses "x" then exit

	   			printf("Keyboard exit requested : exiting now - bye!\n");
	   			keepProcessing = false;
		  } else if (key == ' '){

			// when we have a suitable model, move to recognition

			printf("Using current contour model for RECOGNITION\n\n");

		  	modelBuilding = false;
	  	  }
	  }

      // destroy window objects
      // (triggered by event loop *only* window is closed)

      cvDestroyAllWindows();

      // destroy image object (if it does not originate from a capture object)

      if (!capture){
		  cvReleaseImage( &img );
      }

	  // destroy image objects

      cvReleaseImage( &grayImg );
	  cvReleaseImage( &thresholdedImg );

      // all OK : main returns 0

      return 0;
    }

    // not OK : main returns -1

    return -1;
}
/******************************************************************************/
