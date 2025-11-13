#!/usr/bin/env python3
import csv
from collections import defaultdict
from statistics import mean


def us_to_ms(us):
    return us / 1_000.0


def us_to_s(us):
    return us / 1_000_000.0


def analyze_metrics(filename):
    results = defaultdict(lambda: {"SND": None, "RCV": None})

    with open(filename, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            def to_int(key):
                try:
                    return int(row.get(key, 0))
                except ValueError:
                    return 0

            row["packets_sent"] = to_int("packets_sent")
            row["packets_received"] = to_int("packets_received")
            row["packets_lost"] = to_int("packets_lost")
            row["packets_duplicate"] = to_int("packets_duplicate")
            row["packets_discarded"] = to_int("packets_discarded")
            row["acks_sent"] = to_int("acks_sent")
            row["acks_received"] = to_int("acks_received")
            row["acks_lost"] = to_int("acks_lost")
            row["retransmits"] = to_int("retransmits")
            row["sender_paused"] = to_int("sender_paused")
            row["start"] = to_int("start")
            row["end"] = to_int("end")
            row["buffer_capacity"] = to_int("buffer_capacity")
            row["payload_len"] = to_int("payload_len")
            row["type"] = row.get("type", "").strip().upper()
            results[row["id"]][row["type"]] = row

    summary = []

    for id_, sides in results.items():
        snd = sides.get("SND")
        rcv = sides.get("RCV")
        if not snd or not rcv:
            continue

        duration_us = snd["end"] - snd["start"]
        duration_s = us_to_s(duration_us)
        duration_ms = us_to_ms(duration_us)

        throughput = snd["payload_len"] / duration_s if duration_s > 0 else 0

        packet_loss = snd["packets_lost"] + rcv["packets_lost"]
        total_packets = snd["packets_sent"] + rcv["packets_received"]
        loss_rate = (packet_loss / total_packets * 100) if total_packets > 0 else 0

        ack_loss_rate = (snd["acks_lost"] + rcv["acks_lost"]) / (
            (snd["acks_sent"] + rcv["acks_received"]) or 1
        ) * 100

        duplicate_rate = (
            (snd["packets_duplicate"] + rcv["packets_duplicate"]) /
            ((rcv["packets_received"]) or 1)
        ) * 100

        discard_rate = (
            (snd["packets_discarded"] + rcv["packets_discarded"]) /
            ((rcv["packets_received"]) or 1)
        ) * 100

        retransmit_rate = (
            snd["retransmits"] / (snd["packets_sent"] or 1)
        ) * 100

        summary.append({
            "id": id_,
            "duration_ms": duration_ms,
            "throughput_Bps": throughput,
            "packet_loss_%": loss_rate,
            "ack_loss_%": ack_loss_rate,
            "duplicate_%": duplicate_rate,
            "discard_%": discard_rate,
            "retransmit_%": retransmit_rate,
            "paused_us": snd["sender_paused"],
            "payload_len": snd["payload_len"],
        })

    print(
        f"{'ID':<20} {'Duration(ms)':>12} {'Throughput(B/s)':>18} "
        f"{'PktLoss(%)':>12} {'AckLoss(%)':>12} {'Dup(%)':>8} "
        f"{'Disc(%)':>8} {'Retr(%)':>8} {'Paused(µs)':>12} {'Payload':>10}"
    )
    print("-" * 120)
    for s in summary:
        print(
            f"{s['id']:<20} "
            f"{s['duration_ms']:>12.2f} "
            f"{s['throughput_Bps']:>18.2f} "
            f"{s['packet_loss_%']:>12.2f} "
            f"{s['ack_loss_%']:>12.2f} "
            f"{s['duplicate_%']:>8.2f} "
            f"{s['discard_%']:>8.2f} "
            f"{s['retransmit_%']:>8.2f} "
            f"{s['paused_us']:>12} "
            f"{s['payload_len']:>10}"
        )

    if summary:
        avg = lambda k: mean(s[k] for s in summary)
        print("\n=== Overall Summary ===")
        print(f"Average Throughput: {avg('throughput_Bps'):.2f} B/s")
        print(f"Average Packet Loss: {avg('packet_loss_%'):.2f}%")
        print(f"Average ACK Loss: {avg('ack_loss_%'):.2f}%")
        print(f"Average Duplicate: {avg('duplicate_%'):.2f}%")
        print(f"Average Discard: {avg('discard_%'):.2f}%")
        print(f"Average Retransmit: {avg('retransmit_%'):.2f}%")
        print(f"Average Sender Paused: {avg('paused_us'):.0f} µs")


if __name__ == "__main__":
    analyze_metrics("metrics.csv")
