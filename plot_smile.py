
import pandas as pd
import matplotlib.pyplot as plt
import glob
import os

search_path = os.path.join("smile_data", "*_smile.csv")
filer = glob.glob(search_path)

if not filer:
    print("Fant ingen filer i 'smile_data/'!")
    exit()

filer.sort()

print(f"Fant {len(filer)} filer. Starter plotting...\n")

for filsti in filer:

    filnavn = os.path.basename(filsti)
    print(f"Plotter: {filnavn}")

    deler = filnavn.split("_")
    ticker = deler[0]
    expiry = deler[1]



    df = pd.read_csv(filsti)
    df = df.sort_values(by="Strike")

    # Filtrer bort ugyldige IV-verdier
    df = df[
        (df["HestonIV"] > 1.0) &
        (df["HestonIV"] < 150.0)
    ]

    if "MarketIV" in df.columns:
        df = df[
            (df["MarketIV"] > 1.0) &
            (df["MarketIV"] < 150.0)
        ]

    if df.empty:
        print(
            f"  -> Hoppet over {filnavn} fordi den ikke hadde "
            f"gyldige IV-data etter filtrering."
        )
        continue

    # =========================================================
    # LAG ÉN FIGUR MED TO SUBPLOTS
    # =========================================================

    fig, axes = plt.subplots(
        1, 2,
        figsize=(15, 6.5)
    )

    # =========================================================
    # SUBPLOT 1: IMPLIED VOLATILITY SMILE
    # =========================================================

    ax1 = axes[0]

    # Market IV
    if "MarketIV" in df.columns:
        ax1.scatter(
            df["Strike"],
            df["MarketIV"],
            color="#e377c2",
            marker="x",
            s=40,
            zorder=3,
            alpha=0.8,
            label="Market IV"
        )

    # Heston IV
    ax1.plot(
        df["Strike"],
        df["HestonIV"],
        linestyle="-",
        color="#1f77b4",
        linewidth=2.5,
        zorder=2,
        label="Heston IV"
    )

    # Historical volatility
    if "HistVol" in df.columns:
        hist_vol = df["HistVol"].iloc[0]

        ax1.plot(
            df["Strike"],
            df["HistVol"],
            linestyle="--",
            color="#ff7f0e",
            linewidth=1.5,
            alpha=0.7,
            zorder=1,
            label=f"Historical Volatility ({hist_vol:.2f} %)"
        )

    ax1.set_title(
        "Implied Volatility Smile",
        fontsize=13,
        fontweight="bold"
    )

    ax1.set_xlabel(
        "Strike",
        fontsize=11
    )

    ax1.set_ylabel(
        "Implied Volatility (%)",
        fontsize=11
    )

    ax1.grid(
        True,
        linestyle=":",
        alpha=0.6
    )

    ax1.legend(
        fontsize=9
    )

    # =========================================================
    # SUBPLOT 2: OPTION PRICE
    # =========================================================

    ax2 = axes[1]

    # Market price
    if "MarketPrice" in df.columns:
        ax2.scatter(
            df["Strike"],
            df["MarketPrice"],
            color="#e377c2",
            marker="x",
            s=40,
            zorder=3,
            alpha=0.8,
            label="Market Price"
        )

    # Black-Scholes price
    if "BSPrice" in df.columns:
        ax2.plot(
            df["Strike"],
            df["BSPrice"],
            linestyle="--",
            color="#ff7f0e",
            linewidth=2.0,
            zorder=2,
            label="Black–Scholes Price"
        )

    # Heston price
    if "HestonPrice" in df.columns:
        ax2.plot(
            df["Strike"],
            df["HestonPrice"],
            linestyle="-",
            color="#1f77b4",
            linewidth=2.5,
            zorder=2,
            label="Heston Price"
        )

    ax2.set_title(
        "Option Price",
        fontsize=13,
        fontweight="bold"
    )

    ax2.set_xlabel(
        "Strike",
        fontsize=11
    )

    ax2.set_ylabel(
        "Call Option Price",
        fontsize=11
    )

    ax2.grid(
        True,
        linestyle=":",
        alpha=0.6
    )

    ax2.legend(
        fontsize=9
    )

    # =========================================================
    # FELLES FIGURTITTEL
    # =========================================================

    fig.suptitle(
        f"{ticker} | Expiry: {expiry}",
        fontsize=15,
        fontweight="bold"
    )

    plt.tight_layout()

    plt.show()

print("\n>>> Ferdig!")

