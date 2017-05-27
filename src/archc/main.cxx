#include <iostream>
#include <fstream>
#include <exception>

using namespace std;

void dispatch(int count, char** arguments);

int main(int ac, char* av[])
{
    try {
        dispatch(ac, av);
    }
    catch(exception& error) {
        cout << "ERROR" << endl << error.what() << endl;
    }
    catch(...) {
        cout << "unknown error\n";
    }
}
