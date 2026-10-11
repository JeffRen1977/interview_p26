# Chapter 12. ML Compilers and Graph Runtimes

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- Part IV · target ~4,500 words · 4 figures · readiness: Medium -->
<!-- Private source notes (write fresh; never paste): company/amazon/23-Neuron*; LLM/算子融合.md; LLM/tensor_lifetime.md -->

> What an ML compiler does: graph lowering, operator scheduling, memory planning. TVM, XLA, TensorRT, ExecuTorch, ONNX Runtime, QNN. Tensor lifetime analysis and memory planning for fixed-budget devices. The compilation pipeline: ONNX → graph IR → schedule → codegen → binary.

## Why interviewers ask this

## What the loops actually ask

- Walk through the compilation pipeline from an ONNX graph to a device binary.
- What is operator fusion, and when does the compiler refuse to fuse?
- A compiled model uses 1.8 GB peak memory. The device has 1.2 GB available. Reduce it without changing the model.

## The mental model

## Worked problems

## Code you can run

<!-- tensor lifetime analysis example -->

## In the room

## Failure modes

## Exercises
