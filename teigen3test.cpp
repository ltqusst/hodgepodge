#include <iostream>
#include "Eigen/Dense"

using namespace std;
using namespace Eigen;

using RowMajorMatrixXf = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;

//=================================================================================================================
//simulate Eigen's comma initializer
//   the virtue of this over normal variadic function(with parameter pack) interface is (like in iostream cases)
//      1. type can be inter-leaved one by one.
//      2. args number is not fixed.
//      3. args can be expanded to include more custom types easily.
struct M3f {
    double m[3][3];
    
    struct Loader {
        M3f& m;
        int i;
        Loader(M3f& m, int i) : m(m), i(i) {}
        ~Loader(){
            if(i != 9)
                std::cout << "dtor:Comma separated initializer expect 9 but got *" << i << "* elements" << std::endl;
        }
        Loader& operator,(double x) {
            set(x);
            i++;
            return *this;
        }
        void set(double x) {if(i < 9) m.m[i/3][i%3] = x;}
    };
    
    Loader operator<<(double x) {
        m[0][0] = x;
        return Loader(*this, 1);
    }
    friend ostream& operator<<(ostream& os, const M3f& m);
};
ostream& operator<<(ostream& os, const M3f& m){
    for(int i=0;i<3;i++){
        for(int j=0;j<3;j++)
            os << m.m[i][j] << " ";
        os << std::endl;
    }
    return os;
}
//=================================================================================================================
// The base case: we just have a single number.
template <typename T>
double sum(T t) {
  return t;
}

// The recursive case: we take a number, alongside
// some other numbers, and produce their sum.
template <typename T, typename... Rest>
double sum(T t, Rest... rest) {
  return t*2 + sum(rest...);
}

class kk{
public:
    std::string  name;
    int id;
    kk(const std::string &name, int id):name(name),id(id){
        std::cout << "kk ctor" << std::endl;
    }
    kk(const kk & m){
        std::cout << "kk copy ctor" << std::endl;
        name = m.name;
        id = m.id;
    }
    kk(const kk && m){
        std::cout << "kk move ctor" << std::endl;
        name = std::move(m.name);
        id = m.id;
    }
    ~kk(){std::cout << "kk dtor" << std::endl;}
    friend ostream& operator<<(ostream& os, const kk& m);
};
ostream& operator<<(ostream& os, const kk& m){
    os << "[" << m.name << ":" << m.id << "]";
}

kk build_kk()
{
    return {std::string(2,'c'),1};
}

//==================================================================================================================
int main()
{
  VectorXd v(3);
  v << 1, 2, 3;
    
    
    M3f a;
    a << 1,2,3,4,5,6,7,8,'A','B';
    cout << a;
    cout << sum(1,2,3) << endl;
    
    cout << "build_kk() returns " << build_kk() << endl;
    
    kk k1 = build_kk();
    kk k2 = std::move(k1); //copy construct
    cout << "k1=" << k1 << endl;
    cout << "k2=" << k2 << endl;

    RowMajorMatrixXf A = RowMajorMatrixXf::Random(3,3);
    RowMajorMatrixXf B = RowMajorMatrixXf::Random(3,2);
    cout << "Here is the invertible matrix A:" << endl << A << endl;
    cout << "Here is the matrix B:" << endl << B << endl;
    RowMajorMatrixXf X = A.lu().solve(B);
    cout << "Here is the (unique) solution X to the equation AX=B:" << endl << X << endl;
    cout << "Relative error: " << (A*X-B).norm() / B.norm() << endl;
}




