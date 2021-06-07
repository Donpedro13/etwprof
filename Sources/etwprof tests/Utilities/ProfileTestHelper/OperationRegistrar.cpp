#include "OperationRegistrar.hpp"

#include <stdexcept>

namespace PTH {

OperationRegistrar& OperationRegistrar::Instance ()
{
    static OperationRegistrar instance;

    return instance;
}

void OperationRegistrar::Register (const std::wstring& name, const Operation& operation)
{
    m_operationMap[name] = operation;
}

const Operation* OperationRegistrar::Find (const std::wstring& name) const
{
    try {
        return &m_operationMap.at (name);
    } catch (const std::out_of_range&) {
        return nullptr;
    }
}

void OperationRegistrar::Enumerate (const Enumerator& enumerator) const
{
    for (auto& pair : m_operationMap) {
        if (!enumerator (pair.first, &pair.second))
            return;
    }
}

OperationRegistrator::OperationRegistrator (const std::wstring& name, const Operation& operation)
{
    OperationRegistrar::Instance ().Register (name, operation);
}

} // namespace PTH