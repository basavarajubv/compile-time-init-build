#pragma once

#include <cib/tuple.hpp>
#include <interrupt/config/fwd.hpp>

namespace interrupt {
template <typename ConfigT, typename... SubIrqImpls>
struct shared_sub_irq_impl {
  public:
    /**
     * True if this shared_irq::impl has any active sub_irq::Impls, otherwise
     * False.
     *
     * This is used to optimize compiled size and runtime performance. Unused
     * irqs should not consume any resources.
     */
    static bool constexpr active = (SubIrqImpls::active or ...);

  private:
    template <typename InterruptHal, bool en>
    constexpr static EnableActionType enable_action =
        ConfigT::template enable_action<InterruptHal, en>;

    constexpr static auto enable_field = ConfigT::enable_field;
    constexpr static auto status_field = ConfigT::status_field;
    using StatusPolicy = typename ConfigT::StatusPolicy;

    cib::tuple<SubIrqImpls...> sub_irq_impls;

  public:
    explicit constexpr shared_sub_irq_impl(SubIrqImpls const &...impls)
        : sub_irq_impls{cib::make_tuple(impls...)} {}

    auto get_interrupt_enables() const {
        if constexpr (active) {
            auto const active_sub_irq_impls =
                cib::filter(sub_irq_impls, [](auto irq) {
                    return decltype(irq)::type::active;
                });

            return cib::apply(
                [&](auto &&...irqs) {
                    return cib::tuple_cat(irqs.get_interrupt_enables()...,
                                          cib::make_tuple(enable_field));
                },
                active_sub_irq_impls);

        } else {
            return cib::make_tuple();
        }
    }

    /**
     * Evaluate interrupt status of each sub_irq::impl and run each one with a
     * pending interrupt. Clear any hardware interrupt pending bits as
     * necessary.
     */
    inline void run() const {
        if constexpr (active) {
            if (apply(read(enable_field(1))) && apply(read(status_field(1)))) {
                StatusPolicy::run([&] { apply(clear(status_field)); },
                                  [&] {
                                      cib::for_each([](auto irq) { irq.run(); },
                                                    sub_irq_impls);
                                  });
            }
        }
    }
};
} // namespace interrupt
