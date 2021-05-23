#include "mpsc_machine.hpp"
#include <iostream>
#include <thread>
#include <tuple>
#include <numeric>

int main()
{
    mpsc_machine<int, std::vector<double> > machine(64);
    std::thread consumer([&machine]() {machine.process(); });
    machine.subscribe(42, [](std::vector<double>&& v) { std::cout << "{" << std::accumulate(v.begin(), v.end(), 0.) << "}\n"; });
    std::thread prod0([&machine]() {
        machine.push(42, { 42., 0., 0., 0. });
        machine.push(42, { 41., 1., 0.42, 0. });
        machine.subscribe(0, [](std::vector<double>&& v) { std::cout << "[" << v.size() << "]\n"; });
        machine.push(0, {});
        machine.push(0, {0.,});
        machine.subscribe(0, [](std::vector<double>&& v) { std::cout << "[[" << std::accumulate(v.begin(), v.end(), 0.) << "]]\n"; });
        // since the handler change was in the same thread -- it will apply to the correct vectors
        machine.push(0, { 0., 0.0001});
        machine.push(0, { 0., 0.0002});
    });
    std::thread prod1([&machine]() {
        machine.push(42, { 0.000042 });
        machine.push(42, { 0.000042042 });
        machine.stop(); // any thread can stop the machine
        });
    prod0.join();
    prod1.join();
    consumer.join();

    // we do not check the return value, since we know that there was only one stop
    machine.flush();
}