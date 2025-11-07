#!/usr/bin/env python3
import csv
from collections import defaultdict
from statistics import mean


def ns_to_ms(ns):
    return ns / 1_000_000.0


def ns_to_s(ns):
    return ns / 1_000_000_000.0


def analyze_metrics(filename):
    results = defaultdict(lambda: {"SND": None, "RCV": None})

    with open(filename, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            row["packets_sent"] = int(row["packets_sent"])
            row["packets_received"] = int(row["packets_received"])
            row["packets_lost"] = int(row["packets_lost"])
            row["acks_sent"] = int(row["acks_sent"])
            row["acks_received"] = int(row["acks_received"])
            row["acks_lost"] = int(row["acks_lost"])
            row["start"] = int(row["start"])
            row["end"] = int(row["end"])
            row["buffer_capacity"] = int(row["buffer_capacity"])
            row["payload_len"] = int(row["payload_len"])
            row["type"] = row["type"].strip().upper()
            results[row["id"]][row["type"]] = row

    summary = []

    for id_, sides in results.items():
        snd = sides.get("SND")
        rcv = sides.get("RCV")
        if not snd or not rcv:
            continue

        duration_ns = snd["end"] - snd["start"]
        duration_s = ns_to_s(duration_ns)
        throughput = snd["payload_len"] / duration_s if duration_s > 0 else 0

        packet_loss = snd["packets_lost"] + rcv["packets_lost"]
        total_packets = snd["packets_sent"] + rcv["packets_received"]
        loss_rate = (packet_loss / total_packets * 100) if total_packets > 0 else 0

        ack_loss_rate = (snd["acks_lost"] + rcv["acks_lost"]) / (
            (snd["acks_sent"] + rcv["acks_received"]) or 1
        ) * 100

        summary.append({
            "id": id_,
            "duration_ms": ns_to_ms(duration_ns),
            "throughput_Bps": throughput,
            "packet_loss_%": loss_rate,
            "ack_loss_%": ack_loss_rate,
            "payload_len": snd["payload_len"],
        })

    print(f"{'ID':<20} {'Duration(ms)':>12} {'Throughput(B/s)':>18} "
          f"{'PktLoss(%)':>12} {'AckLoss(%)':>12} {'Payload':>10}")
    print("-" * 90)
    for s in summary:
        print(f"{s['id']:<20} "
              f"{s['duration_ms']:>12.2f} "
              f"{s['throughput_Bps']:>18.2f} "
              f"{s['packet_loss_%']:>12.2f} "
              f"{s['ack_loss_%']:>12.2f} "
              f"{s['payload_len']:>10}")

    # Global stats
    if summary:
        avg_throughput = mean(s["throughput_Bps"] for s in summary)
        avg_loss = mean(s["packet_loss_%"] for s in summary)
        avg_ack_loss = mean(s["ack_loss_%"] for s in summary)
        print("\n=== Overall Summary ===")
        print(f"Average Throughput: {avg_throughput:.2f} B/s")
        print(f"Average Packet Loss: {avg_loss:.2f}%")
        print(f"Average ACK Loss: {avg_ack_loss:.2f}%")


if __name__ == "__main__":
    analyze_metrics("metrics.csv")
