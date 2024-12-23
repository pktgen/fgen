# FGEN - Frame Generator

## Overview

Frame Generator (FGEN) is a collection of userspace libraries for accelerating packet
processing for cloud applications. It aims to provide better performance than that of standard
network socket interfaces by taking advantage of platform technologies such as Intel(R) AVX-512,
Intel(R) DSA, CLDEMOTE, etc. The I/O layer is primarily built on AF_XDP, an interface that
delivers packets straight to userspace, bypassing the kernel networking stack. FGEN provides ways
to expose metrics and telemetry with examples to deploy network services on Kubernetes.

## FGEN Consumers

* **Cloud Network Function (CNF) and Cloud Application developers**: Those who create applications
  based on FGEN. FGEN hides the low-level I/O, allowing the developer to focus on their
  application.

* **CNF and Cloud Application consumers**: Those who consume the applications developed by the CNF
  developer. FGEN showcases deployment models for their applications using Kubernetes.

## FGEN Characteristics

FGEN follows a set of principles:

* **Functionality**: Provide a framework for cloud native developers that offers full control of
  their application.

* **Usability**: Simplify cloud native application development to enable the developer to create
  applications by providing APIs that abstract the complexities of the underlying system while
  still taking advantage of acceleration features when available.

* **Interoperability**: The FGEN framework is built primarily on top of AF_XDP. Other interfaces,
  such as memif, are also supported, however building on AF_XDP ensures it is possible to move
  an application across environments wherever AF_XDP is supported.

* **Portability/stability**: FGEN provides ABI stability and a common API to access network
  interfaces.

* **Performance**: Take advantage of platform technologies to accelerate packet processing or
  fall-back to software when acceleration is unavailable.

* **Observability**: Provide observability into the performance and operation of the application.

* **Security**: Security for deployment in a cloud environment is critical.

## FGEN background

FGEN was created to enable cloud native developers to use AF_XDP and other interfaces in a simple
way while providing better performance as compared to standard Linux networking interfaces.

FGEN does not replace DPDK (Data Plane Development Kit), which provides the highest performance for
packet processing. DPDK implements user space drivers, bypassing the kernel drivers. This approach
of rewriting drivers is one reason DPDK achieves the highest performance for packet processing. DPDK
also implements a framework to initialize and setup platform resources i.e. scanning PCI bus for
devices, allocating memory via hugepages, setting up Primary/Secondary process support, etc.

In contrast to DPDK, FGEN does not have custom drivers. Instead it expects the kernel drivers to
implement AF_XDP, preferably in zero-copy mode. Since there are no PCIe drivers, there's no PCI bus
scanning, and does not require physically contiguous and pinned memory. This simplifies deployment
for cloud native applications while gaining the performance benefits provided by AF_XDP.

## FGEN notable directories

The following shows a subset of the directory structure.

```bash
.
├── docs                      # Documentation APIs, guides, getting started, ...
├── examples                  # Example applications to understand how to use FGEN features
├── lib                       # Set of libraries for building FGEN applications
│   ├── core                  # Core libraries for FGEN
│   ├── include               # FGEN includes
│   ├── log                   # Log system
│   ├── mmap                  # memory map allocator
│   ├── osal                  # OS abstraction layer
│   └── utils                 # Misc utils
├── test                      # Unit test framework
    ├── common                #
    └── app                   # Functional unit testing application
```
