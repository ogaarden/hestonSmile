 #include <iostream>

#include <vector>

#include <cmath>

#include <algorithm>

#include <numeric>

#include <fstream>

#include <filesystem>


#include "params.hpp"

#include "models.hpp"


namespace fs = std::filesystem;


// 1. FORBEDRET LOSS-FUNKSJON MED RELATIV FEIL OG MYK STRAFF

double hestonLoss(const std::vector<double>& params, double S, double T, double r,

                  const std::vector<double>& strikes, const std::vector<double>& market_prices) {

    double kappa   = params[0];

    double theta   = params[1];

    double sigma_v = params[2];

    double rho     = params[3];

    double v0      = params[4];


    // Glatt straffefunksjon (Penalty) i stedet for hard 1e12

    double penalty = 0.0;

    if (kappa < 0.01)   penalty += std::pow(0.01 - kappa, 2) * 1e4;

    if (theta < 0.001)  penalty += std::pow(0.001 - theta, 2) * 1e4;

    if (sigma_v < 0.01) penalty += std::pow(0.01 - sigma_v, 2) * 1e4;

    if (v0 < 0.001)     penalty += std::pow(0.001 - v0, 2) * 1e4;

    if (rho < -0.98)    penalty += std::pow(-0.98 - rho, 2) * 1e4;

    if (rho > 0.98)     penalty += std::pow(rho - 0.98, 2) * 1e4;


    // Feller-betingelse (2*kappa*theta > sigma_v^2) - myk straff

    double feller_diff = (sigma_v * sigma_v) - (2.0 * kappa * theta);

    if (feller_diff > 0.0) {

        penalty += feller_diff * 50.0;

    }


    if (penalty > 1e5) return penalty;


    double total_error = 0.0;

    size_t n_strikes = strikes.size();

   

    for (size_t i = 0; i < n_strikes; ++i) {

        // NY (send med 0.0 for q fordi r allerede er r - q):

double model_price = hestonCOS(S, strikes[i], T, r, 0.0, v0, kappa, theta, sigma_v, rho);

       

        if (std::isnan(model_price) || std::isinf(model_price) || model_price <= 0.0) {

            return 1e8;

        }


        // RELATIV FEIL: Vektlegger OTM og ITM like mye for å fange opp IV-smile/skew

        double weight = std::max(market_prices[i], 0.1); // Unngå divisjon på 0

        double rel_diff = (market_prices[i] - model_price) / weight;

       

        total_error += rel_diff * rel_diff;

    }


    return total_error + penalty;

}


struct SimplexPoint {

    std::vector<double> coords;

    double loss;

};


// 2. KORRIGERT ENKELT NELDER-MEAD SØK

std::vector<double> runNelderMeadForCandidate(const std::vector<double>& start_x, double S, double T, double r,

                                             const std::vector<double>& strikes, const std::vector<double>& market_prices) {

    size_t dim = 5;

    size_t num_pts = dim + 1;

    std::vector<SimplexPoint> simplex(num_pts);


    std::vector<double> steps = {

        std::abs(start_x[0]) * 0.2 + 0.05,

        std::abs(start_x[1]) * 0.2 + 0.01,

        std::abs(start_x[2]) * 0.2 + 0.05,

        -0.05,

        std::abs(start_x[4]) * 0.2 + 0.01

    };


    simplex[0].coords = start_x;

    simplex[0].loss = hestonLoss(start_x, S, T, r, strikes, market_prices);

   

    for (size_t i = 0; i < dim; ++i) {

        std::vector<double> next_coords = start_x;

        next_coords[i] += steps[i];

        simplex[i + 1].coords = next_coords;

        simplex[i + 1].loss = hestonLoss(next_coords, S, T, r, strikes, market_prices);

    }


    for (int iter = 0; iter < 500; ++iter) {

        size_t lo = 0, hi = 0;

        for (size_t i = 1; i < num_pts; ++i) {

            if (simplex[i].loss < simplex[lo].loss) lo = i;

            if (simplex[i].loss > simplex[hi].loss) hi = i;

        }


        std::vector<double> centroid(dim, 0.0);

        for (size_t i = 0; i < num_pts; ++i) {

            if (i == hi) continue;

            for (size_t j = 0; j < dim; ++j) centroid[j] += simplex[i].coords[j];

        }

        for (size_t j = 0; j < dim; ++j) centroid[j] /= dim;


        // --- REFLEKSJON ---

        std::vector<double> reflected_coords(dim);

        for (size_t j = 0; j < dim; ++j) {

            reflected_coords[j] = centroid[j] + 1.0 * (centroid[j] - simplex[hi].coords[j]);

        }

        double reflected_loss = hestonLoss(reflected_coords, S, T, r, strikes, market_prices);


        // RETTET BUG: Sammenligner mot simplex[lo].loss (beste punkt)

        if (reflected_loss < simplex[lo].loss) {

            // --- EKSPANSJON ---

            std::vector<double> expanded_coords(dim);

            for (size_t j = 0; j < dim; ++j) {

                expanded_coords[j] = centroid[j] + 2.0 * (reflected_coords[j] - centroid[j]);

            }

            double expanded_loss = hestonLoss(expanded_coords, S, T, r, strikes, market_prices);

           

            if (expanded_loss < reflected_loss) {

                simplex[hi].coords = expanded_coords;

                simplex[hi].loss = expanded_loss;

            } else {

                simplex[hi].coords = reflected_coords;

                simplex[hi].loss = reflected_loss;

            }

        } else if (reflected_loss < simplex[hi].loss) {

            simplex[hi].coords = reflected_coords;

            simplex[hi].loss = reflected_loss;

        } else {

            // --- SAMMENTREKNING ---

            std::vector<double> contracted_coords(dim);

            for (size_t j = 0; j < dim; ++j) {

                contracted_coords[j] = centroid[j] + 0.5 * (simplex[hi].coords[j] - centroid[j]);

            }

            double contracted_loss = hestonLoss(contracted_coords, S, T, r, strikes, market_prices);

           

            if (contracted_loss < simplex[hi].loss) {

                simplex[hi].coords = contracted_coords;

                simplex[hi].loss = contracted_loss;

            } else {

                // --- SKRUMP ---

                for (size_t i = 0; i < num_pts; ++i) {

                    if (i == lo) continue;

                    for (size_t j = 0; j < dim; ++j) {

                        simplex[i].coords[j] = simplex[lo].coords[j] + 0.5 * (simplex[i].coords[j] - simplex[lo].coords[j]);

                    }

                    simplex[i].loss = hestonLoss(simplex[i].coords, S, T, r, strikes, market_prices);

                }

            }

        }

    }


    size_t best_idx = 0;

    for (size_t i = 1; i < num_pts; ++i) {

        if (simplex[i].loss < simplex[best_idx].loss) best_idx = i;

    }

   

    return simplex[best_idx].coords;

}


// 3. HOVEDFUNKSJON MED MULTI-START OPTIMERING

std::vector<double> calibrateHeston(double S, double T, double r,

                                    const std::vector<double>& strikes,

                                    const std::vector<double>& market_prices) {

   

    std::vector<std::vector<double>> candidates = {

        {1.5, 0.04, 0.3, -0.7, 0.04}, // Moderat vol (~20%)

        {2.5, 0.09, 0.5, -0.6, 0.09}, // Høyere vol (~30%)

        {0.8, 0.16, 0.4, -0.8, 0.16}, // Skjevt smil / høy vol (~40%)

        {2.0, 0.02, 0.2, -0.5, 0.02}, // Lav vol (~14%)

        {3.5, 0.25, 0.6, -0.4, 0.20}  // Høy vol-of-vol

    };


    std::vector<double> global_best_params = candidates[0];

    double global_best_loss = 1e18;


    // Kjører Nelder-Mead fra ALLE kandidatene og velger den som finner det laveste global-minimumet

    for (const auto& cand : candidates) {

        std::vector<double> opt_params = runNelderMeadForCandidate(cand, S, T, r, strikes, market_prices);

        double loss = hestonLoss(opt_params, S, T, r, strikes, market_prices);

       

        if (loss < global_best_loss) {

            global_best_loss = loss;

            global_best_params = opt_params;

        }

    }


    return global_best_params;

} 