#include "Python.h"

int
main(int argc, char *argv[])
{
  Py_Initialize();
  PyRun_SimpleString("print 'Hello, World'");
  Py_Finalize();

  return 0;
}
