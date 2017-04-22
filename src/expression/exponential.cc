/*
 * Copyright (C) 2014-2017 Olzhas Rakhimov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// @file exponential.cc
/// Implementation of various exponential expressions.

#include "exponential.h"

#include <cmath>

#include "src/error.h"

namespace scram {
namespace mef {

namespace {  // Poisson process probability evaluators.

/// Negative exponential law probability for Poisson process.
///
/// @param[in] lambda  The rate of the process.
/// @param[in] time  The time lapse since the last observation.
///
/// @returns The probability that the process has occurred by the given time.
double p_exp(double lambda, double time) {
  return 1 - std::exp(-lambda * time);
}

/// The probability description for two consecutive Poisson processes
/// starting one after another.
/// In other words,
/// the second process is dependent upon the first process.
///
/// @param[in] p_mu  The probability of the first process (dependency).
/// @param[in] p_lambda  The probability of the second process (dependent).
/// @param[in] mu  The rate of the first process.
/// @param[in] lambda  The rate of the second process.
/// @param[in] time  The time period under question.
///
/// @returns The probability that the second process has occurred.
double p_exp(double p_mu, double p_lambda, double mu, double lambda,
             double time) {
  return lambda == mu ? p_lambda - (1 - p_lambda) * lambda * time
                      : (lambda * p_mu - mu * p_lambda) / (lambda - mu);
}

}  // namespace

ExponentialExpression::ExponentialExpression(Expression* lambda, Expression* t)
    : Expression({lambda, t}),
      lambda_(*lambda),
      time_(*t) {}

void ExponentialExpression::Validate() const {
  EnsureNonNegative<InvalidArgument>(&lambda_, "rate of failure");
  EnsureNonNegative<InvalidArgument>(&time_, "mission time");
}

double ExponentialExpression::Mean() noexcept {
  return p_exp(lambda_.Mean(), time_.Mean());
}

double ExponentialExpression::DoSample() noexcept {
  return p_exp(lambda_.Sample(), time_.Sample());
}

GlmExpression::GlmExpression(Expression* gamma, Expression* lambda,
                             Expression* mu, Expression* t)
    : Expression({gamma, lambda, mu, t}),
      gamma_(*gamma),
      lambda_(*lambda),
      mu_(*mu),
      time_(*t) {}

void GlmExpression::Validate() const {
  EnsurePositive<InvalidArgument>(&lambda_, "rate of failure");
  EnsureNonNegative<InvalidArgument>(&mu_, "rate of repair");
  EnsureNonNegative<InvalidArgument>(&time_, "mission time");
  EnsureProbability<InvalidArgument>(&gamma_, "failure on demand");
}

double GlmExpression::Mean() noexcept {
  return Compute(gamma_.Mean(), lambda_.Mean(), mu_.Mean(), time_.Mean());
}

double GlmExpression::DoSample() noexcept {
  return Compute(gamma_.Sample(), lambda_.Sample(), mu_.Sample(),
                 time_.Sample());
}

double GlmExpression::Compute(double gamma, double lambda, double mu,
                              double time) noexcept {
  double r = lambda + mu;
  return (lambda - (lambda - gamma * r) * std::exp(-r * time)) / r;
}

WeibullExpression::WeibullExpression(Expression* alpha, Expression* beta,
                                     Expression* t0, Expression* time)
    : Expression({alpha, beta, t0, time}),
      alpha_(*alpha),
      beta_(*beta),
      t0_(*t0),
      time_(*time) {}

void WeibullExpression::Validate() const {
  EnsurePositive<InvalidArgument>(&alpha_,
                                  "scale parameter for Weibull distribution");
  EnsurePositive<InvalidArgument>(&beta_,
                                  "shape parameter for Weibull distribution");
  EnsureNonNegative<InvalidArgument>(&t0_, "time shift");
  EnsureNonNegative<InvalidArgument>(&time_, "mission time");
}

double WeibullExpression::Compute(double alpha, double beta,
                                  double t0, double time) noexcept {
  return time <= t0 ? 0 : 1 - std::exp(-std::pow((time - t0) / alpha, beta));
}

double WeibullExpression::Mean() noexcept {
  return Compute(alpha_.Mean(), beta_.Mean(), t0_.Mean(), time_.Mean());
}

double WeibullExpression::DoSample() noexcept {
  return Compute(alpha_.Sample(), beta_.Sample(), t0_.Sample(), time_.Sample());
}

PeriodicTest::PeriodicTest(Expression* lambda, Expression* tau,
                           Expression* theta, Expression* time)
    : Expression({lambda, tau, theta, time}),
      flavor_(new PeriodicTest::InstantRepair(lambda, tau, theta, time)) {}

PeriodicTest::PeriodicTest(Expression* lambda, Expression* mu,
                           Expression* tau, Expression* theta,
                           Expression* time)
    : Expression({lambda, mu, tau, theta, time}),
      flavor_(new PeriodicTest::InstantTest(lambda, mu, tau, theta, time)) {}

PeriodicTest::PeriodicTest(Expression* lambda, Expression* lambda_test,
                           Expression* mu, Expression* tau, Expression* theta,
                           Expression* gamma, Expression* test_duration,
                           Expression* available_at_test, Expression* sigma,
                           Expression* omega, Expression* time)
    : Expression({lambda, lambda_test, mu, tau, theta, gamma, test_duration,
                  available_at_test, sigma, omega, time}),
      flavor_(new PeriodicTest::Complete(
          lambda, lambda_test, mu, tau, theta, gamma, test_duration,
          available_at_test, sigma, omega, time)) {}

void PeriodicTest::InstantRepair::Validate() const {
  EnsurePositive<InvalidArgument>(&lambda_, "rate of failure");
  EnsurePositive<InvalidArgument>(&tau_, "time between tests");
  EnsureNonNegative<InvalidArgument>(&theta_, "time before tests");
  EnsureNonNegative<InvalidArgument>(&time_, "mission time");
}

void PeriodicTest::InstantTest::Validate() const {
  InstantRepair::Validate();
  EnsureNonNegative<InvalidArgument>(&mu_, "rate of repair");
}

void PeriodicTest::Complete::Validate() const {
  InstantTest::Validate();
  EnsureNonNegative<InvalidArgument>(&lambda_test_,
                                     "rate of failure while under test");
  EnsurePositive<InvalidArgument>(&test_duration_,
                                  "duration of the test phase");
  EnsureProbability<InvalidArgument>(&gamma_, "failure at test start");
  EnsureProbability<InvalidArgument>(&sigma_, "failure detection upon test");
  EnsureProbability<InvalidArgument>(&omega_, "failure at restart");

  if (test_duration_.Mean() > tau_.Mean())
    throw InvalidArgument(
        "The test duration must be less than the time between tests.");
  if (test_duration_.interval().upper() > tau_.interval().lower())
    throw InvalidArgument(
        "The sampled test duration must be less than the time between tests.");
}

double PeriodicTest::InstantRepair::Compute(double lambda, double tau,
                                            double theta,
                                            double time) noexcept {
  if (time <= theta)  // No test has been performed.
    return p_exp(lambda, time);
  double delta = time - theta;
  double time_after_test = delta - static_cast<int>(delta / tau) * tau;
  return p_exp(lambda, time_after_test ? time_after_test : tau);
}

double PeriodicTest::InstantRepair::Mean() noexcept {
  return Compute(lambda_.Mean(), tau_.Mean(), theta_.Mean(), time_.Mean());
}

double PeriodicTest::InstantRepair::Sample() noexcept {
  return Compute(lambda_.Sample(), tau_.Sample(), theta_.Sample(),
                 time_.Sample());
}

double PeriodicTest::InstantTest::Compute(double lambda, double mu, double tau,
                                          double theta, double time) noexcept {
  if (time <= theta)  // No test has been performed.
    return p_exp(lambda, time);

  // Carry fraction from probability of a previous period.
  auto carry = [&lambda, &mu](double p_lambda, double p_mu, double t) {
    // Probability of failure after repair.
    double p_mu_lambda = p_exp(p_lambda, p_mu, lambda, mu, t);
    return 1 - p_mu + p_mu_lambda - p_lambda;
  };

  double prob = p_exp(lambda, theta);  // The current rolling probability.
  auto p_period = [&prob, &carry](double p_lambda, double p_mu, double t) {
    return prob * carry(p_lambda, p_mu, t) + p_lambda;
  };

  double delta = time - theta;
  int num_periods = delta / tau;
  double fraction = carry(p_exp(lambda, tau), p_exp(mu, tau), tau);
  double compound = std::pow(fraction, num_periods);  // Geometric progression.
  prob = prob * compound + p_exp(lambda, tau) * (compound - 1) / (fraction - 1);

  double time_after_test = delta - num_periods * tau;
  return p_period(p_exp(lambda, time_after_test), p_exp(mu, time_after_test),
                  time_after_test);
}

double PeriodicTest::InstantTest::Mean() noexcept {
  return Compute(lambda_.Mean(), mu_.Mean(), tau_.Mean(), theta_.Mean(),
                 time_.Mean());
}

double PeriodicTest::InstantTest::Sample() noexcept {
  return Compute(lambda_.Sample(), mu_.Sample(), tau_.Sample(), theta_.Sample(),
                 time_.Sample());
}

double PeriodicTest::Complete::Compute(double lambda, double lambda_test,
                                       double mu, double tau, double theta,
                                       double gamma, double test_duration,
                                       bool available_at_test, double sigma,
                                       double omega, double time) noexcept {
  if (time <= theta)  // No test has been performed.
    return p_exp(lambda, time);

  double p_fail = p_exp(lambda, theta);
  double p_repair = 0;
  double p_available = 1 - p_fail;

  // Failure after repair.
  auto p_mu_lambda = [&](double p_mu, double p_lambda, double t) {
    return p_mu * omega + (1 - omega) * p_exp(p_mu, p_lambda, mu, lambda, t);
  };

  auto p_test = [&](double p_lambda_test, double p_mu, double p_lambda,
                    double t, bool available = true) {
    double p_fail_transient =
        p_fail + p_available * (gamma + (1 - gamma) * p_lambda_test);
    p_fail = p_repair * p_mu_lambda(p_mu, p_lambda, t) +
             (1 - sigma) * p_fail_transient;
    p_repair = (1 - p_mu) * p_repair + sigma * p_fail_transient;
    p_available =
        1 - p_fail - p_repair -
        (available ? 0 : p_available * (1 - gamma) * (1 - p_lambda_test));
    assert(p_repair >= 0 && p_repair <= 1);
    assert(p_fail >= 0 && p_fail <= 1);
    assert(p_available >= 0);
  };
  // Time after test.
  auto p_period = [&](double p_lambda, double p_mu, double t) {
    p_fail = p_available * p_lambda + p_fail +
             p_repair * p_mu_lambda(p_mu, p_lambda, t);
    p_repair = p_repair * (1 - p_mu);
    p_available = 1 - p_fail - p_repair;
    assert(p_repair >= 0 && p_repair <= 1);
    assert(p_fail >= 0 && p_fail <= 1);
    assert(p_available >= 0);
  };

  double delta = time - theta;
  int num_periods = delta / tau;
  double delta_period = tau - test_duration;

  double p_lambda_test = p_exp(lambda_test, test_duration);
  double p_lambda_at_test = p_exp(lambda, test_duration);
  double p_mu_at_test = p_exp(mu, test_duration);

  double p_lambda = p_exp(lambda, delta_period);
  double p_mu = p_exp(mu, delta_period);

  for (int i = 0; i < num_periods; ++i) {
    p_test(p_lambda_test, p_mu_at_test, p_lambda_at_test, test_duration);
    p_period(p_lambda, p_mu, delta_period);
  }

  double time_after_test = delta - num_periods * tau;
  if (time_after_test <= test_duration) {
    p_test(p_exp(lambda_test, time_after_test), p_exp(mu, time_after_test),
           p_exp(lambda, time_after_test), time_after_test, available_at_test);

  } else {
    p_test(p_lambda_test, p_mu_at_test, p_lambda_at_test, test_duration);
    double leftover_time = time_after_test - test_duration;
    p_period(p_exp(lambda, leftover_time), p_exp(mu, leftover_time),
             leftover_time);
  }
  assert(p_available >= 0 && p_available <= 1);
  return 1 - p_available;
}

double PeriodicTest::Complete::Mean() noexcept {
  return Compute(lambda_.Mean(), lambda_test_.Mean(), mu_.Mean(), tau_.Mean(),
                 theta_.Mean(), gamma_.Mean(), test_duration_.Mean(),
                 available_at_test_.Mean(), sigma_.Mean(), omega_.Mean(),
                 time_.Mean());
}

double PeriodicTest::Complete::Sample() noexcept {
  return Compute(lambda_.Sample(), lambda_test_.Sample(), mu_.Sample(),
                 tau_.Sample(), theta_.Sample(), gamma_.Sample(),
                 test_duration_.Sample(), available_at_test_.Sample(),
                 sigma_.Sample(), omega_.Sample(), time_.Sample());
}

}  // namespace mef
}  // namespace scram
