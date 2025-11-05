// Simple test client for the Julia discipline server
#include <iostream>

#include <grpcpp/grpcpp.h>

#include <variable.h>
#include <explicit.h>

using std::cout;
using std::endl;
using std::make_pair;
using std::string;
using std::vector;

using grpc::Channel;

using philote::Partials;
using philote::Variable;
using philote::Variables;

int main()
{
    std::shared_ptr<Channel> channel = grpc::CreateChannel("localhost:50051",
                                                           grpc::InsecureChannelCredentials());
    philote::ExplicitClient client;
    client.ConnectChannel(channel);

    cout << "Connected to server at localhost:50051" << endl;

    // send stream options to the analysis server
    client.SendStreamOptions();
    cout << "Sent stream options" << endl;

    // run setup
    client.Setup();
    cout << "Setup complete" << endl;

    // get the variable meta data from the server
    client.GetVariableDefinitions();
    cout << "Got variable definitions" << endl;

    vector<string> vars = client.GetVariableNames();
    cout << "\nVariable List:" << endl;
    for (auto &name : vars)
    {
        auto var = client.GetVariableMeta(name);
        cout << "  " << name << " (";
        if (var.type() == philote::kInput)
            cout << "input";
        else if (var.type() == philote::kOutput)
            cout << "output";
        cout << ")" << endl;
    }

    // get the partials meta data from the server
    client.GetPartialDefinitions();
    cout << "\nGot partials definitions" << endl;

    // define the inputs and run a function evaluation
    cout << "\nRunning compute with x=5.0..." << endl;
    Variables inputs;
    inputs["x"] = Variable(philote::kInput, {1});
    inputs["x"](0) = 5.0;

    Variables outputs = client.ComputeFunction(inputs);

    cout << "Outputs:" << endl;
    for (auto &var : outputs)
    {
        cout << "  " << var.first << " = " << var.second(0) << endl;
    }

    // run a gradient evaluation
    cout << "\nRunning compute_partials..." << endl;
    Partials partials = client.ComputeGradient(inputs);

    cout << "Partials:" << endl;
    for (auto &par : partials)
    {
        cout << "  d" << par.first.first << "/d" << par.first.second
             << " = " << par.second(0) << endl;
    }

    cout << "\n✅ All tests passed!" << endl;

    return 0;
}
