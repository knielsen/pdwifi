#include "Python.h"

const char *script_source= "stuff.py";
const char *sample_text= "Kristian, dette er Kristians hjemmeside";

int
main(int argc, char *argv[])
{
  Py_Initialize();

  PyObject *main_module= PyImport_AddModule("__main__");
  PyObject *main_dict= PyModule_GetDict(main_module);

  FILE *fh = fopen(script_source, "r");
  if (!fh)
  {
    fprintf(stderr, "Fatal: cannot open file '%s'.\n", script_source);
    exit(1);
  }
  PyRun_File(fh, script_source, Py_file_input, main_dict, main_dict);
  fclose(fh);

  PyObject *processor_class= PyDict_GetItemString(main_dict, "Processor");
  if (processor_class)
  {
    PyObject *processor_object= PyInstance_New(processor_class, NULL, NULL);
    if (processor_object)
    {
      PyObject *result= PyObject_CallMethod(processor_object, "process", "s",
                                            sample_text);
      if (result)
      {
        const char *output= PyString_AsString(result);
        printf("Result: '%s'\n", output);
        Py_DECREF(result);
      }
      Py_DECREF(processor_object);
    }
    Py_DECREF(processor_class);
  }

  Py_DECREF(main_dict);

  Py_Finalize();

  return 0;
}
