#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
import sys
import os

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} <metrics.csv>")
    sys.exit(1)

path = sys.argv[1]
if not os.path.exists(path):
    print(f"File not found: {path}")
    sys.exit(1)

df = pd.read_csv(path)

for col in [
    "packets_duplicate",
    "packets_discarded",
    "retransmits",
    "sender_paused",
]:
    if col not in df.columns:
        df[col] = 0

df["duration"] = (df["end"] - df["start"]) / 1_000_000

df["throughput_pkts"] = df["packets_sent"] / df["duration"]
df["throughput_bytes"] = (df["packets_sent"] * df["payload_len"]) / df["duration"]

df["loss_rate"] = df["packets_lost"] / df["packets_sent"].replace(0, 1)
df["duplicate_rate"] = df["packets_duplicate"] / df["packets_received"].replace(0, 1)
df["discard_rate"] = df["packets_discarded"] / df["packets_received"].replace(0, 1)
df["retransmit_rate"] = df["retransmits"] / df["packets_sent"].replace(0, 1)

senders = df[df["type"] == "SND"]
receivers = df[df["type"] == "RCV"]

plt.figure(figsize=(10, 6))
plt.title("Packets Sent vs Received")
plt.bar(senders["id"], senders["packets_sent"], label="Sent", alpha=0.7)
plt.bar(receivers["id"], receivers["packets_received"], label="Received", alpha=0.7)
plt.legend()
plt.xticks(rotation=45, ha="right")
plt.tight_layout()
plt.show()

plt.figure(figsize=(10, 6))
plt.title("Throughput (packets/sec)")
plt.bar(df["id"], df["throughput_pkts"], color="orange")
plt.xticks(rotation=45, ha="right")
plt.tight_layout()
plt.show()

plt.figure(figsize=(10, 6))
plt.title("Packet Loss Rate")
plt.bar(df["id"], df["loss_rate"] * 100, color="red")
plt.ylabel("%")
plt.xticks(rotation=45, ha="right")
plt.tight_layout()
plt.show()

plt.figure(figsize=(10, 6))
plt.title("Retransmit, Duplicate, and Discard Rates")
width = 0.25
x = range(len(df))
plt.bar([i - width for i in x], df["retransmit_rate"] * 100, width, label="Retransmit")
plt.bar(x, df["duplicate_rate"] * 100, width, label="Duplicate")
plt.bar([i + width for i in x], df["discard_rate"] * 100, width, label="Discard")
plt.ylabel("%")
plt.xticks(x, df["id"], rotation=45, ha="right")
plt.legend()
plt.tight_layout()
plt.show()

plt.figure(figsize=(10, 6))
plt.title("Sender Paused (µs)")
plt.bar(df["id"], df["sender_paused"])
plt.xticks(rotation=45, ha="right")
plt.tight_layout()
plt.show()

