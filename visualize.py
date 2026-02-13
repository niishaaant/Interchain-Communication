
import json
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def load_jsonl(file_path):
    data = []
    with open(file_path, 'r') as f:
        for line in f:
            data.append(json.loads(line))
    return pd.DataFrame(data)

def plot_aggregate_metrics(metrics_df):
    plt.figure(figsize=(12, 8))
    metrics_df['name'].value_counts().plot(kind='bar')
    plt.title('Aggregate Metrics')
    plt.xlabel('Metric Name')
    plt.ylabel('Count')
    plt.xticks(rotation=45, ha='right')
    plt.tight_layout()
    plt.savefig('aggregate_metrics.png')
    plt.close()

def plot_latency_distribution(ibc_events_df, transactions_df):
    ibc_events_df['ts'] = pd.to_datetime(ibc_events_df['ts'])
    transactions_df['ts'] = pd.to_datetime(transactions_df['ts'])

    # IBC Packet Latency
    packet_created = ibc_events_df[ibc_events_df['event'] == 'packet_created'].set_index('tx_id')
    ack_received = ibc_events_df[ibc_events_df['event'] == 'ack_received'].set_index('tx_id')
    ibc_latency = (ack_received['ts'] - packet_created['ts']).dt.total_seconds().dropna()

    plt.figure(figsize=(12, 6))
    plt.hist(ibc_latency, bins=50, alpha=0.7, label='IBC Packet Latency')
    plt.title('IBC Packet Latency Distribution')
    plt.xlabel('Latency (seconds)')
    plt.ylabel('Frequency')
    plt.legend()
    plt.grid(True)
    plt.savefig('ibc_latency_distribution.png')
    plt.close()

    # Transaction Latency
    tx_created = transactions_df[transactions_df['event'] == 'created'].set_index('tx_id')
    tx_finalized = transactions_df[transactions_df['event'] == 'included_in_block'].set_index('tx_id')
    tx_latency = (tx_finalized['ts'] - tx_created['ts']).dt.total_seconds().dropna()

    plt.figure(figsize=(12, 6))
    plt.hist(tx_latency, bins=50, alpha=0.7, label='Transaction Latency')
    plt.title('Transaction Latency Distribution')
    plt.xlabel('Latency (seconds)')
    plt.ylabel('Frequency')
    plt.legend()
    plt.grid(True)
    plt.savefig('transaction_latency_distribution.png')
    plt.close()

def plot_throughput_and_latency(metrics_df, ibc_events_df):
    metrics_df['ts'] = pd.to_datetime(metrics_df['ts'])
    metrics_df.set_index('ts', inplace=True)

    # Throughput
    throughput = metrics_df[metrics_df['name'] == 'tx_submitted'].resample('S').sum()['delta']
    plt.figure(figsize=(12, 6))
    throughput.plot()
    plt.title('Transaction Throughput (tx/sec)')
    plt.xlabel('Time')
    plt.ylabel('Transactions per Second')
    plt.grid(True)
    plt.savefig('throughput.png')
    plt.close()

    # Latency over time
    ibc_events_df['ts'] = pd.to_datetime(ibc_events_df['ts'])
    packet_created = ibc_events_df[ibc_events_df['event'] == 'packet_created'].set_index('tx_id')
    ack_received = ibc_events_df[ibc_events_df['event'] == 'ack_received'].set_index('tx_id')
    ibc_latency_df = pd.DataFrame({
        'created_ts': packet_created['ts'],
        'latency': (ack_received['ts'] - packet_created['ts']).dt.total_seconds()
    }).dropna()

    plt.figure(figsize=(12, 6))
    plt.plot(ibc_latency_df['created_ts'], ibc_latency_df['latency'], marker='.', linestyle='-')
    plt.title('IBC Packet Latency Over Time')
    plt.xlabel('Time')
    plt.ylabel('Latency (seconds)')
    plt.grid(True)
    plt.savefig('latency_over_time.png')
    plt.close()

if __name__ == '__main__':
    metrics_df = load_jsonl('metrics.jsonl')
    ibc_events_df = load_jsonl('ibc_events.jsonl')
    transactions_df = load_jsonl('transactions.jsonl')

    plot_aggregate_metrics(metrics_df.copy())
    plot_latency_distribution(ibc_events_df.copy(), transactions_df.copy())
    plot_throughput_and_latency(metrics_df.copy(), ibc_events_df.copy())

    print("Generated the following plots:")
    print("- aggregate_metrics.png")
    print("- ibc_latency_distribution.png")
    print("- transaction_latency_distribution.png")
    print("- throughput.png")
    print("- latency_over_time.png")
