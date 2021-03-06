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

#ifndef OR_TOOLS_GLOP_LP_SOLVER_H_
#define OR_TOOLS_GLOP_LP_SOLVER_H_

#include "base/unique_ptr.h"

#include "glop/parameters.pb.h"
#include "glop/preprocessor.h"
#include "lp_data/lp_data.h"
#include "lp_data/lp_types.h"
#include "util/time_limit.h"

namespace operations_research {
namespace glop {

// A full-fledged linear programming solver.
class LPSolver {
 public:
  LPSolver();

  // Sets and gets the solver parameters.
  // See the proto for an extensive documentation.
  void SetParameters(const GlopParameters& parameters);
  const GlopParameters& GetParameters() const;

  // Solves the given linear program and returns the solve status. See the
  // ProblemStatus documentation for a description of the different values.
  //
  // The solution can be retrieved afterwards using the getter functions below.
  // Note that depending on the returned ProblemStatus the solution values may
  // not mean much, so it is important to check the returned status.
  //
  // Incrementality: From one Solve() call to the next, the internal state is
  // not cleared and the solver may take advantage of its current state if the
  // given lp is only slightly modified. If the modification is too important,
  // or if the solver does not see how to reuse the previous state efficiently,
  // it will just solve the problem from scratch. On the other hand, if the lp
  // is the same, calling Solve() again should basically resume the solve from
  // the last position. To disable this behavior, simply call Clear() before.
  ProblemStatus Solve(const LinearProgram& lp) MUST_USE_RESULT;

  // Puts the solver in a clean state.
  //
  // Calling Solve() for the first time, or calling Clear() then Solve() on the
  // same problem is guaranted to be deterministic and to always give the same
  // result, assuming that no time limit was specified.
  void Clear();

  // This loads a given solution and computes related quantities so that the
  // getters below will refer to it.
  //
  // Depending on the given solution status, this also checks the solution
  // feasibility or optimality. The exact behavior and tolerances are controlled
  // by the solver parameters. Because of this, the returned ProblemStatus may
  // be changed from the one passed in the ProblemSolution to ABNORMAL or
  // IMPRECISE. Note that this is the same logic as the one used by Solve() to
  // verify the solver solution.
  ProblemStatus LoadAndVerifySolution(const LinearProgram& lp,
                                      const ProblemSolution& solution);

  // Returns the objective value of the solution with its offset and scaling.
  Fractional GetObjectiveValue() const;

  // Accessors to information related to variables.
  const DenseRow& variable_values() const { return primal_values_; }
  const DenseRow& reduced_costs() const { return reduced_costs_; }
  const VariableStatusRow& variable_statuses() const {
    return variable_statuses_;
  }

  // Accessors to information related to constraints. The activity of a
  // constraint is the sum of its linear terms evaluated with variables taking
  // their values at the current solution.
  //
  // Note that the dual_values() do not take into account an eventual objective
  // scaling of the solved LinearProgram.
  const DenseColumn& dual_values() const { return dual_values_; }
  const DenseColumn& constraint_activities() const {
    return constraint_activities_;
  }
  const ConstraintStatusColumn& constraint_statuses() const {
    return constraint_statuses_;
  }

  // Returns the primal maximum infeasibility of the solution.
  // This indicates by how much the variable and constraint bounds are violated.
  Fractional GetMaximumPrimalInfeasibility() const;

  // Returns the dual maximum infeasibility of the solution.
  // This indicates by how much the variable costs (i.e. objective) should be
  // modified for the solution to be an exact optimal solution.
  Fractional GetMaximumDualInfeasibility() const;

  // Returns true if the solution status was OPTIMAL and it seems that there is
  // more than one basic optimal solution. Note that this solver always returns
  // an optimal BASIC solution and that there is only a finite number of them.
  // Moreover, given one basic solution, since the basis is always refactorized
  // at optimality before reporting the numerical result, then all the
  // quantities (even the floating point ones) should be always the same.
  //
  // TODO(user): Test this behavior extensively if a client relies on it.
  bool MayHaveMultipleOptimalSolutions() const;

  // Returns the number of simplex iterations used by the last Solve().
  int GetNumberOfSimplexIterations() const;

  // Returns the "deterministic time" since the creation of the solver. Note
  // That this time is only increased when some operations take place in this
  // class.
  //
  // TODO(user): Currently, this is only modified when the simplex code is
  // executed.
  //
  // TODO(user): Improve the correlation with the running time.
  double DeterministicTime() const;

 private:
  // Resizes all the solution vectors to the given sizes.
  // This is used in case of error to make sure all the getter functions will
  // not crash when given row/col inside the initial linear program dimension.
  void ResizeSolution(RowIndex row, ColIndex col);

  // Make sure the primal and dual values are within their bounds in order to
  // have a strong guarantee on the optimal solution. See
  // provide_strong_optimal_guarantee in the GlopParameters proto.
  void MovePrimalValuesWithinBounds(const LinearProgram& lp);
  void MoveDualValuesWithinBounds(const LinearProgram& lp);

  // Runs all preprocessors in sequence.
  void RunPreprocessors(const TimeLimit& time_limit);

  // Runs the given preprocessor and pushes it when relevant (i.e. when it did
  // something) on the preprocessors_ stack.
  void RunAndPushIfRelevant(std::unique_ptr<Preprocessor> preprocessor,
                            const std::string& name, const TimeLimit& time_limit);

  // Runs the revised simplex algorithm if needed (i.e. if the program was not
  // already solved by the preprocessors).
  void RunRevisedSimplexIfNeeded(ProblemSolution* solution);

  // Postprocess the solution by calling the StoreSolution() of the
  // preprocessors in the reverse order in which their where applied.
  void PostprocessSolution(ProblemSolution* solution);

  // Checks that the returned solution values and statuses are consistent.
  // Returns true if this is the case. See the code for the exact check
  // performed.
  bool IsProblemSolutionConsistent(const LinearProgram& lp,
                                   const ProblemSolution& solution) const;

  // Returns true if there may be multiple optimal solutions.
  // The return value is true if:
  // - a non-fixed variable, at one of its boumds, has its reduced
  //   cost close to zero.
  // or if:
  // - a non-equality constraint (i.e. l <= a.x <= r, with l != r), is at one of
  //   its bounds (a.x = r or a.x = l) and has its dual value close to zero.
  bool IsOptimalSolutionOnFacet(const LinearProgram& lp);

  // Computes derived quantities from the solution.
  void ComputeReducedCosts(const LinearProgram& lp);
  void ComputeConstraintActivities(const LinearProgram& lp);

  // Computes the primal/dual objectives (without the offset). Note that the
  // dual objective needs the reduced costs in addition to the dual values.
  double ComputeObjective(const LinearProgram& lp);
  double ComputeDualObjective(const LinearProgram& lp);

  // Given a relative precision on the primal values of up to
  // solution_feasibility_tolerance(), this returns an upper bound on the
  // expected precision of the objective.
  double ComputeMaxExpectedObjectiveError(const LinearProgram& lp);

  // Returns the max absolute cost pertubation (resp. rhs perturbation) so that
  // the pair (primal values, dual values) is an EXACT optimal solution to the
  // perturbed problem. Note that this assumes that
  // MovePrimalValuesWithinBounds() and MoveDualValuesWithinBounds() have
  // already been called. The Boolean is_too_large is set to true if any of the
  // perturbation exceed the tolerance (which depends of the coordinate).
  //
  // These bounds are computed using the variable and constraint statuses by
  // enforcing the complementary slackness optimal conditions. Note that they
  // are almost the same as ComputeActivityInfeasibility() and
  // ComputeReducedCostInfeasibility() but looks for optimality rather than just
  // feasibility.
  //
  // Note(user): We could get EXACT bounds on these perturbations by changing
  // the rounding mode appropriately during these computations. But this is
  // probably not needed.
  Fractional ComputeMaxCostPerturbationToEnforceOptimality(
      const LinearProgram& lp, bool* is_too_large);
  Fractional ComputeMaxRhsPerturbationToEnforceOptimality(
      const LinearProgram& lp, bool* is_too_large);

  // Computes the maximum of the infeasibilities associated with each values.
  // The returned infeasibilities are the maximum of the "absolute" errors of
  // each vector coefficients.
  //
  // These function also set is_too_large to true if any infeasibility is
  // greater than the tolerance (which depends of the coordinate).
  double ComputePrimalValueInfeasibility(const LinearProgram& lp,
                                         bool* is_too_large);
  double ComputeActivityInfeasibility(const LinearProgram& lp,
                                      bool* is_too_large);
  double ComputeDualValueInfeasibility(const LinearProgram& lp,
                                       bool* is_too_large);
  double ComputeReducedCostInfeasibility(const LinearProgram& lp,
                                         bool* is_too_large);

  // Dimension of the linear program given to the last Solve().
  // This is used for displaying purpose only.
  EntryIndex initial_num_entries_;
  RowIndex initial_num_rows_;
  ColIndex initial_num_cols_;

  // On a call to Solve(), this is initialized to an exact copy of the given
  // linear program. It is later modified by the preprocessors and then solved
  // by the revised simplex.
  LinearProgram current_linear_program_;

  // Stack of preprocessors currently applied to the current linear program.
  std::vector<std::unique_ptr<Preprocessor>> preprocessors_;

  // The revised simplex solver.
  std::unique_ptr<RevisedSimplex> revised_simplex_;

  // The number of revised simplex iterations used by the last Solve().
  int num_revised_simplex_iterations_;

  // The current ProblemSolution.
  // TODO(user): use a ProblemSolution directly?
  ProblemStatus status_;
  DenseRow primal_values_;
  DenseColumn dual_values_;
  VariableStatusRow variable_statuses_;
  ConstraintStatusColumn constraint_statuses_;

  // Quantities computed from the solution and the linear program.
  DenseRow reduced_costs_;
  DenseColumn constraint_activities_;
  Fractional problem_objective_value_;
  bool may_have_multiple_solutions_;
  Fractional max_absolute_primal_infeasibility_;
  Fractional max_absolute_dual_infeasibility_;

  // Proto holding all the parameters of the algorithm.
  GlopParameters parameters_;

  // The number of times Solve() was called. Used to number dump files.
  int num_solves_;

  DISALLOW_COPY_AND_ASSIGN(LPSolver);
};

}  // namespace glop
}  // namespace operations_research

#endif  // OR_TOOLS_GLOP_LP_SOLVER_H_
