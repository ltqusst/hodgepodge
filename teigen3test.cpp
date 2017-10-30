#include <iostream>
#include "Eigen/Dense"

using namespace std;
using namespace Eigen;

using RowMajorMatrixXf = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

int main()
{
  VectorXd v(3);
  v << 1, 2, 3;
    
    cout << v << endl;
      
    RowMajorMatrixXf A = RowMajorMatrixXf::Random(3,3);
    RowMajorMatrixXf B = RowMajorMatrixXf::Random(3,2);
    cout << "Here is the invertible matrix A:" << endl << A << endl;
    cout << "Here is the matrix B:" << endl << B << endl;
    RowMajorMatrixXf X = A.lu().solve(B);
    cout << "Here is the (unique) solution X to the equation AX=B:" << endl << X << endl;
    cout << "Relative error: " << (A*X-B).norm() / B.norm() << endl;
}

