#include <stdio.h>
#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <unistd.h>

using namespace std;

void CoutThreadPrint(int i) {
   cout << i << " Hello World!\n"; 
}

void PrintfThreadPrint(int i) {
    printf("%d Hello World!\n", i);
}

enum PrintMode { PRINTF, COUT, UNSET }; 

int main(int argc, char* argv[]) {

    int opt;
    PrintMode printType = UNSET;
    int threadNum = 0;

    while ((opt = getopt(argc, argv, "n:t:")) != -1) {
        switch (opt) {
        case 'n':
            threadNum = std::atoi(optarg);
            break;
        case 't':
            string tmpMode = string(optarg);
            printType = (tmpMode == "printf") ? PRINTF : COUT;
            break;
        }
    }
    
    void (*print_func_ptr)(int);

    if (threadNum < 1){
        cout << "Less than 1 thread specified! Exiting...";
        return 1;
    }

    switch (printType) {
        case PRINTF: 
            print_func_ptr = PrintfThreadPrint;
            break;
        case COUT:
            print_func_ptr = CoutThreadPrint;
            break;
        default: 
            cout << "Error reading print mode parameter." << endl;
            return 1;
    }

    thread * threadList[threadNum];
    
    // Create all threads for the amount passed via CLI and set them to print via the specified type.
    for (int threadId = 0; threadId < threadNum; threadId++) {
        threadList[threadId] = new thread(print_func_ptr, threadId);
    }

    for (int i = 0; i < threadNum; i++) {
        threadList[i] -> join();
    }

    return 0;
}

