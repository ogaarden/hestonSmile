#ifndef PARAMS_HPP
#define PARAMS_HPP

#include <vector>

std::vector<double> calibrateHeston(double S, double T, double r, 
                                    const std::vector<double>& strikes, 
                                    const std::vector<double>& market_prices);

#endif