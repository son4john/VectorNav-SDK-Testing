#include <iostream>
#include "Python.h"
#include <curl/curl.h> 
#include <map> 
#include <vector> 

using namespace std;

struct predictionResult {
    string category, questionType; 
};

typedef struct predictionResult Struct; 

Struct findpredictionResult(string& question) { 
    Struct pr; 

    Py_Initialize();

    PyObject* sysPath = PySys_GetObject("path");
    PyList_Append(sysPath, PyUnicode_DecodeFSDefault("/Users/alenjo/Documents/SurveyFill"));
    PyObject* pModule = PyImport_ImportModule("surveyAnalyze");
    if (pModule != NULL) {
        PyObject* pFunc = PyObject_GetAttrString(pModule, "analyze");

        if (pFunc && PyCallable_Check(pFunc)) {
            const char* surveyQuestion = question;
            PyObject* pArg = Py_BuildValue("(s)", surveyQuestion);

            PyObject* pValue = PyObject_CallObject(pFunc, pArg);
            

            if (pValue != NULL) {
                PyObject *pStr1 = PyTuple_GetItem(pValue, 0);
                PyObject *pStr2 = PyTuple_GetItem(pValue, 1);

                const char *str1 = PyUnicode_AsUTF8(pStr1);
                const char *str2 = PyUnicode_AsUTF8(pStr2);

                pr.category = str1; 
                pr.questionType = str2; 

                // Clean up
                Py_DECREF(pValue);
            } else {
                PyErr_Print();
            }

            Py_DECREF(pFunc);
            Py_DECREF(pArg);
        } else {
            if (PyErr_Occurred())
                PyErr_Print();
            std::cerr << "Cannot find function 'analyze'" << std::endl;
        }

        Py_DECREF(pModule);
    } else {
        PyErr_Print();
        std::cerr << "Failed to load Python module" << std::endl;
    }

    Py_Finalize();
    
    return pr;
}

int main() {
    string profile = grabfInfoProfile(); 
    
    // Initialize the Python Module to run the trained prediction model
    Struct modelpr;
    string question = "What is your age?";
    modelpr = findpredictionResult(question);
    cout << "Model predicted the category as " + modelpr.category << endl;
    cout << "Model predicted the question type as " + modelpr.questionType << endl; 
}
