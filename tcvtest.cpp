#include "opencv2/opencv.hpp"

using namespace cv;

int main()
{
	Mat m = Mat::zeros(480,960,CV_32FC1);

	for(auto i=m.begin<float>(); i!= m.end<float>(); ++i){
		*i = 0.5;
	}
	
	imshow("m",m);
	waitKey(0);
}

