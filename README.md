# UAS Communication Component

This C++ application runs in conjunction with a Tangram-generated LMCP component software interface, and OpenAMASE (a simulation environment for testing of UAV control technologies). 

It sends serialized LMCP `AirVehicleState` and `AirVehicleConfiguration` messages to OpenAMASE to visualize the behavior of the system.

_This is a resource for Tangram Pro™ Tutorials which can be found [here](https://docs.tangramflex.io/docs/tutorials/getting_started)._

# Requirements

- A Linux [**Ubuntu 20.04 LTS**](https://ubuntu.com/download/desktop) machine (or VM) is required to run this application.
- The [OpenAMSE](https://github.com/afrl-rq/OpenAMASE) Java program must be installed.

## Dependencies

Additional dependencies must be generated before the `lmcp_sender` application can be built.

Follow these tutorials to design and generate the necessary LMCP component software interface in Tangram Pro™.

- [Visualize a Component Based System Design
](https://docs.tangramflex.io/docs/tutorials/visualize_a_Component_based_system_design)
- [Generate Code with Workflows](https://docs.tangramflex.io/docs/tutorials/generating_code_with_workflows)

Lastly, follow this tutorial to implement and utilize this application.
- [Implement Your Component Software Interface (CSI)](https://docs.tangramflex.io/docs/tutorials/tutorials/implement_csi)

## Building lmcp_sender
After the Tangram-generated code is in place, build the app with the command `make`

## Running lmcp_sender
Run the app with the command `./lmcp_sender`
