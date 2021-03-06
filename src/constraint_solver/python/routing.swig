// Copyright 2010-2014 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// TODO(user): Refactor this file to adhere to the SWIG style guide.

%include "constraint_solver/python/constraint_solver.swig"

// Include the file we want to wrap a first time.
%{
#include "constraint_solver/routing.h"
%}

// As with the solver ctor, at the beginning of the routing model's
// constructor code (in python), call SetFlags() with the python flags
// as parameters.
// Note: Indentation is critical here as the code is copied verbatim in the
// python code.
%feature("pythonprepend") operations_research::RoutingModel::RoutingModel %{
        Solver.SetFlags(FLAGS.cp_trace_propagation,
                        FLAGS.cp_trace_search,
                        FLAGS.cp_print_model,
                        FLAGS.cp_model_stats,
                        FLAGS.cp_export_file,
                        FLAGS.cp_no_solve,
                        FLAGS.cp_profile_file)
%}


// Convert RoutingModel::NodeIndex to (32-bit signed) integers.
%typemap(in) operations_research::RoutingModel::NodeIndex {
  $1 = operations_research::RoutingModel::NodeIndex(PyInt_AsLong($input));
}
%typemap(out) operations_research::RoutingModel::NodeIndex {
  $result = PyInt_FromLong($1.value());
}

// Convert std::vector<RoutingModel::NodeIndex> to/from int arrays.
%{
template<>
bool PyObjAs(PyObject *py, operations_research::RoutingModel::NodeIndex* i) {
  int temp;
  if (!PyObjAs(py, &temp)) return false;
  *i = operations_research::RoutingModel::NodeIndex(temp);
  return true;
}
%}
PY_LIST_OUTPUT_TYPEMAP(operations_research::RoutingModel::NodeIndex,
                       PyInt_Check, PyInt_FromLong);
// TODO(user): also support std::vector<std::vector<>> <-> list of list.

// Create input mapping for NodeEvaluator2
%{
static int64 PyCallback2NodeIndexNodeIndex(
    PyObject* pyfunc,
    operations_research::RoutingModel::NodeIndex i,
    operations_research::RoutingModel::NodeIndex j) {
  int64 result = 0;
  // Cast to int needed, no int64 support
  PyObject* arglist = Py_BuildValue("ll",
                                    i.value<int>(),
                                    j.value<int>());
  PyObject* pyresult = PyEval_CallObject(pyfunc, arglist);
  Py_DECREF(arglist);
  if (pyresult) {
    result = PyInt_AsLong(pyresult);
  }
  Py_XDECREF(pyresult);
  return result;
}
%}
%typemap(in) operations_research::RoutingModel::NodeEvaluator2* {
  if (!PyCallable_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Need a callable object!");
    SWIG_fail;
  }
  $1 = NewPermanentCallback(&PyCallback2NodeIndexNodeIndex, $input);
}
// Create conversion of vectors of NodeEvaluator2
%{
template<>
bool PyObjAs(PyObject* py_obj,
             operations_research::RoutingModel::NodeEvaluator2** b) {
  if (!PyCallable_Check(py_obj)) {
    PyErr_SetString(PyExc_TypeError, "Need a callable object!");
    return false;
  }
  *b = NewPermanentCallback(&PyCallback2NodeIndexNodeIndex, py_obj);
  return true;
}
%}
// Passing an empty parameter as converter is ok here since no API outputs
// a vector of NodeEvaluator2*.
PY_LIST_OUTPUT_TYPEMAP(operations_research::RoutingModel::NodeEvaluator2*,
                       PyCallable_Check, );

%ignore operations_research::RoutingModel::AddVectorDimension(
    const int64* values,
    int64 capacity,
    const std::string& name);

%ignore operations_research::RoutingModel::AddMatrixDimension(
    const int64* const* values,
    int64 capacity,
    const std::string& name);

%extend operations_research::RoutingModel {
  void AddVectorDimension(const std::vector<int64>& values,
                          int64 capacity,
                          bool fix_start_cumul_to_zero,
                          const std::string& name) {
    DCHECK_EQ(values.size(), $self->nodes());
    $self->AddVectorDimension(values.data(), capacity,
                             fix_start_cumul_to_zero, name);
  }
}

%ignore operations_research::RoutingModel::WrapIndexEvaluator(
    Solver::IndexEvaluator2* evaluator);

%ignore operations_research::RoutingModel::RoutingModel(
    int nodes, int vehicles,
    const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end);

// Wrap cp includes
%include "constraint_solver/routing.h"
