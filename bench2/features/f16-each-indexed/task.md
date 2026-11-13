# Each indexed

Standard input has exactly two lines: names separated by single spaces, then integer scores separated by single spaces. Either line may be empty.

Print `names:`, then one line `I NAME` per name (I counts from 1), then `scores:`, then one line `I SCORE` per score, then `total T` with the sum of the scores.

Write one generic function `each_indexed` that takes a list of any element type and a function, and calls the function with each 1-based index and element in order. It must work both with a function that prints and with one that does not: `each_indexed` declares no fixed effects of its own but takes on those of the function it is given. Produce all three parts of the output through `each_indexed` (the total through a function that only accumulates).
