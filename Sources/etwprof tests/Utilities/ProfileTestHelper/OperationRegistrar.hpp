#ifndef PTH_OPERATION_REGISTRAR_HPP
#define PTH_OPERATION_REGISTRAR_HPP

#include <functional>
#include <string>
#include <unordered_map>

namespace PTH {

using Operation = std::function<bool (void)>;

class OperationRegistrar {
public:
	using Enumerator = std::function<bool (const std::wstring&, const Operation*)>;

	static OperationRegistrar& Instance ();

	void Register (const std::wstring& name, const Operation& operation);

	const Operation* Find (const std::wstring& name) const;

	void Enumerate (const Enumerator& enumerator) const;

private:
	std::unordered_map<std::wstring, Operation> m_operationMap;
};

// Small helper class for registering operations
class OperationRegistrator {
public:
	OperationRegistrator (const std::wstring& name, const Operation& operation);
};

}   // namespace PTH

#endif  // #ifndef PTH_OPERATION_REGISTRAR_HPP