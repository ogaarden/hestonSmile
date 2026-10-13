## Some introduction 

A problem with the black scholes is that is does not put a premium on risk, i.e. options far OTM are more expensive on the free market, than the formula
would suggest. There are ways to combat this, for example by setting volatility to follow a mean reverting OU- SDE (or as some call it : CIR - for Cox-Ingersoll-Ross) instead of being purely deterministic, which gives the
Heston SDE. This program tries to plot the volatility smile on the options chain, using the Heston SDE as a means to price the options. The Heston SDE also has its weakness,
for example volatility seems to shrink while vol-of-vol is high, which in fact does not make sense. This is party fixed in other models, such as SABR model - Or the new
volterra volatility model. In essence the only "unknown" of pricing an option today, is the volatility, as all the other params are given. Therefore volatility modelling
is a large field.


I wanted to find params of the heston model using PINNs (Physics informed neural networks), however was unable to do it. Theres a paper on pricing american options with that exact method here : 
https://arxiv.org/html/2605.06688v1

A great thing about the Heston SDE is that it has a semi closed solution, i.e. we can get an analytical expression for the characteristic function, but the probability integrals has to be solved numerically. Which is what is implemented here, its called HestonCos-method. A paper for more information is here :
https://www.sciencedirect.com/science/article/pii/S0096300322000212



## Issues with the program

The heston model does catch a volatility smile, however its not aggresive enough. I think letting the params file have a higher volatility is worth looking into, or reducing the rho value. Otherwise I think the best case is to use machine learning on each stock, and get params from ML. But as I said I have not done it.


## How to run!

First in "download_data.py", write the tickers you want to analyze in the tickers list, then run the program.
Then you compile and run "volatility_smile.cpp" 

#compiler arg : 
g++ -std=c++17 -Iinclude volatility_smile.cpp src/*.cpp -o volatility_smile.exe

This program asks you to write a ticker, this has to be a ticker that you just downloaded data from using "download_data.py"

Finally, you can run "plot_smile.py", and check wether the project was an absolute fail or decent enough! In the left plot it compares IV of the heston, to market, to historical vol. On the right side it compares what the option price would be using these volatilities into its respective formulas. 

