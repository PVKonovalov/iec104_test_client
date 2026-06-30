//
// Created by Pavel Konovalov on 25/12/2024.
//

#ifndef CONTEXT_H
#define CONTEXT_H
#include <atomic>

class Context {
public:
    std::atomic_bool context;
    Context() { context = true; }
    ~Context() { context = false; }
    void Cancel() { context = false; }
};

inline auto context = new Context();
#endif //CONTEXT_H
