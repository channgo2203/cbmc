#include <cassert>

//typedef int asdf;

class myclass
{
  int x;
public:
  myclass(int _x) : x(_x) {}
  operator int () { return x; }  
//  operator asdf () { return x+1; }  //not allowed
};


int main(int argc, char *argv[]) 
{
  int y;
  myclass a(y);
  int z = (signed int)a;
  
  assert(y == z);

  return 0;
}

