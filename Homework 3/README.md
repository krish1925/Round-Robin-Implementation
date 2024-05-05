# CS CM121/221 Homework 3

This repository contains solutions for Homework 3 of CS CM121/221. The homework is divided into two main problems, each with subproblems outlined below.

## Problem 1: MLE with Many Knowns

Consider an adaptation of the model used in class where samples in the control group follow a Poisson distribution under the null model, while samples in the experimental group follow a Poisson distribution under an alternative model. The subproblems are as follows:

- **(a)** Write down the full likelihood of a gene across all the data.
- **(b)** Find the Maximum Likelihood Estimation (MLE) of δ and verify it.
- **(c)** Interpret the MLE δ ˆ in the context of the knowns.

## Problem 2: Normalization under a Simple Generative Model

This problem deals with normalization under a simple generative model for the number of RNA molecules observed. The subproblems are as follows:

- **(a)** Simulate X j with given parameters and create scatterplots of raw counts.
- **(b)** Implement the DESeq-style normalization function.
- **(c)** Make histograms of normalized values for samples j = {1, 3} and interpret.
- **(d)** Normalize values and create a scatterplot of normalized samples.
- **(e)** Estimate normalization factors with different parameters and compare results.
- **(f)** Simulate X j with new parameters and compare normalization results.
- **(g)** Concatenate matrices from (e) and (f), rerun normalization, and compare results.
