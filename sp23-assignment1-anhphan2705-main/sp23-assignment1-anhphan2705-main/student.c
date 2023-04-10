#include "student.h"
#include <stdbool.h>
#include <stdio.h>

int largest(int array[], int length) { 
  // Set a temporary max value
  int max = array[0];
  // Loop through the array to find a bigger value than current max value
  for (int i = 1; i < length; i++)
  {
    if (array[i] > max) 
    {
      max = array[i];
    }
  }
  return max;
}

int sum(int array[], int length) {
  int sum = 0;
  // Loop through the array and add each value to the current sum
  for (int i = 0; i < length; i++)
  {
    sum += array[i];
  }
  return sum;
}

void swap(int *a, int *b) {
  int temp = *a;
  *a = *b;
  *b = temp;
}

void rotate(int *a, int *b, int *c) {
  // Swap a = c
  int temp = *a;
  *a = *c;
  // Swap b = a
  int  temp_2 = *b;
  *b = temp;
  // Swap c = b
  *c = temp_2;
}

void sort(int array[], int length) {
  // Bubble sort used
  int i, j;
  for (i = 0; i < length - 1; i++)
  {
    // Rearranging the 2 adjacent elements in correct order
    for (j = 0; j < length - i - 1; j++)
    {
      if (array[j] > array[j + 1])
      {
        swap(&array[j], &array[j + 1]);
      }
    }
  }
}

void double_primes(int array[], int length) {
  for (int i = 0; i < length; i++)
  {
    int value = array[i];
    if (value >= 2)
    {
      // Assuming the value is prime by default
      bool prime = true;
      for (int div = 2; div <= value/2; div++)
      {
        // If the value is divisible then set prime to false
        if (value % div == 0)
        {
          prime = false;
          break;
        }
      }
      // Double the value if prime is true
      if (prime){
        array[i] = value * 2;
      }
    }
  }
}

int power(int value, int pow_val){
  // A power function (value = value ^ pow_val)
  int count = 0;
  int result = 1;
  while (count < pow_val)
  {
    result *= value;
    ++count;
  }

  return result;
}

void negate_armstrongs(int array[], int length) {
  for (int i = 0; i < length; i++)
  {
    int remainder, sum = 0;
    int originalNum = array[i];
    int num = originalNum;
    int num_of_digit = 0;
    // Counting the number of digits
    while (num != 0) {
      ++num_of_digit;
      num /= 10;
    } 
    // Finding the sum
    num = originalNum;
    while (num > 0)
    {
      remainder = num % 10;
      sum = sum + power(remainder, num_of_digit);
      num = num / 10;
    }
    // Negating identified Armstrong numbers
    if (sum == originalNum)
    {
      if (array[i] > 0) 
      {
        array[i] = array[i] * (-1);
      }
    }
  }
}
