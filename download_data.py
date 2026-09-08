import yfinance as yf
import os
import pandas as pd

tickers = ["AAPL", "MSFT", "TSLA","AMZN"]

os.makedirs("data/strikes", exist_ok=True)
os.makedirs("data/prices", exist_ok=True)

for t in tickers:
    print(f"\n==================================================")
    print(f"Henter data for {t}...")
    stock = yf.Ticker(t)

    hist = stock.history(period="3mo")
    if not hist.empty:
        price_file_path = f"data/prices/{t.lower()}_daily.csv"

        export_df = hist[['Close', 'High', 'Low', 'Open', 'Volume']].copy()
        
       
        with open(price_file_path, "w") as f:
            f.write("YFinance Hentet Data\n")
            f.write("HPC Synkronisert Prisfil\n")
            f.write("Date,Close,High,Low,Open,Volume\n") # Ekte header (linje 3)
            

        export_df.to_csv(price_file_path, mode='a', header=False)
        print(f"  -> [SPOT OK] Lagret synkronisert prisdata. Siste spot: {export_df['Close'].iloc[-1]:.2f}")
    else:
        print(f"  -> [ADVARSEL] Kunne ikke hente prisdata for {t}")


    os.makedirs(f"data/strikes/{t}", exist_ok=True)

    for exp in stock.options:
        try:
            chain = stock.option_chain(exp)
            rows = []


            for _, row in chain.calls.iterrows():
                bid = row.get("bid", 0.0)
                ask = row.get("ask", 0.0)
                last_price = row.get("lastPrice", 0.0)
                mid = (bid + ask) / 2.0 if (bid + ask) > 0 else last_price
                rows.append((row["strike"], exp, "call", bid, ask, mid))

            for _, row in chain.puts.iterrows():
                bid = row.get("bid", 0.0)
                ask = row.get("ask", 0.0)
                last_price = row.get("lastPrice", 0.0)
                mid = (bid + ask) / 2.0 if (bid + ask) > 0 else last_price
                rows.append((row["strike"], exp, "put", bid, ask, mid))

            rows.sort(key=lambda x: x[0])

            file_path = f"data/strikes/{t}/{t}_{exp}.csv"
            valid_rows = [r for r in rows if r[5] > 0.0]

            if len(valid_rows) > 0:
                with open(file_path, "w") as f:
                    f.write("strike,expiry,type,bid,ask,market_price\n")
                    for strike, expiry, opt_type, bid, ask, market_price in valid_rows:
                        f.write(f"{strike},{expiry},{opt_type},{bid:.4f},{ask:.4f},{market_price:.4f}\n")
                print(f"     -> Lagret {len(valid_rows)} opsjoner i {t}_{exp}.csv")
                        
        except Exception as e:
            print(f"     Kunne ikke hente utløp {exp} for {t}: {e}")

print("\n>>> priser og opsjoner er nå  synkronisert")