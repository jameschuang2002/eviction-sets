import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def plot_histogram(csv_file, deduction_value):
    # Load CSV
    df = pd.read_csv(csv_file, header=None)
    numbers = df[0] - deduction_value  # Deduct parameter
    
    numbers //= 100000000
    # Normalize to start from 0
    min_value = numbers.min()
    numbers -= min_value

    # Get unique values
    unique_numbers = sorted(numbers.unique())

    # Create binary representation
    max_value = unique_numbers[-1] if unique_numbers else 0
    binary_representation = np.zeros(max_value + 1)
    for num in unique_numbers:
        binary_representation[num] = 1  # Mark listed numbers as 1

    # Plot histogram
    plt.bar(range(len(binary_representation)), binary_representation, color='blue')
    plt.xlabel('Number')
    plt.ylabel('Presence (1 for listed, 0 for unlisted)')
    plt.title('Binary Histogram of Numbers')
    plt.savefig('out.png')

# Example usage
plot_histogram('out1.csv', 1200476621398457)  # Replace 'numbers.csv' with your actual file name

