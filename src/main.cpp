#include <iostream>

int main(int argc, char* argv[]){
    if(argc<2){
        std::cout << "Please pass name through cmd" << std::endl;
        return 1;
    }
    std::cout << "Hello, mister " << argv[1] << "!" << std::endl;
    return 0;
}
