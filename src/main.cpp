#include<iostream>
#include"Process.h"
// comments
int main(){
    Process proc("ls", {"-l", "-a"});
    int result = proc.run();
    std::cout << "Process exited with code: " << result << std::endl;   
    
    return 0;
}