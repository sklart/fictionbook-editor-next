#pragma once
#include <atlbase.h>
#include <atlcom.h>
#include <memory>
#include <unordered_map>

namespace FbeBinary {
// The document is the sole owner. Attached DOM behaviors have weak handles;
// neither an element nor a script retaining an old element keeps bytes alive.
class BinaryPayloadStore {
public:
    struct State {
        std::unordered_map<unsigned long long, std::shared_ptr<const CComVariant>> values;
        std::unordered_map<unsigned long long, std::shared_ptr<const CComVariant>> history;
        std::unordered_map<IUnknown*, unsigned long long> identities;
        unsigned long long next = 0, revision = 0;
    };
    // Undo managers/DOM nodes can survive their editor window. History owns
    // only this lease; bytes remain owned by the live document state.
    struct RetainedPayload {
        std::weak_ptr<State> state;
        unsigned long long key = 0;
        ~RetainedPayload() { if (auto owner = state.lock()) owner->history.erase(key); }
        std::shared_ptr<const CComVariant> Get() const {
            const auto owner = state.lock();
            if (!owner) return {};
            const auto found = owner->history.find(key);
            return found == owner->history.end() ? nullptr : found->second;
        }
    };
    BinaryPayloadStore() : m_state(std::make_shared<State>()) {}
    HRESULT Attach(IDispatch* element);
    HRESULT AdoptLegacy(IDispatch* element);
    std::shared_ptr<const CComVariant> Snapshot(IDispatch* element) const;
    HRESULT Assign(IDispatch* element, const std::shared_ptr<const CComVariant>& value);
    std::shared_ptr<RetainedPayload> Retain(const std::shared_ptr<const CComVariant>& value) {
        auto lease = std::make_shared<RetainedPayload>();
        lease->state = m_state; lease->key = ++m_state->next;
        m_state->history.emplace(lease->key, value);
        return lease;
    }
    void Reset() { m_state = std::make_shared<State>(); }
    unsigned long long Revision() const { return m_state->revision; }
    size_t Count() const { return m_state->values.size(); }
private:
    std::shared_ptr<State> m_state;
};
}
