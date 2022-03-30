#include "processor.h"
#include "triggers.h"

namespace triggers {

mcontrol_t::mcontrol_t() :
  type(2), maskmax(0), select(false), timing(false), chain(false),
  match(MATCH_EQUAL), m(false), h(false), s(false), u(false), execute(false),
  store(false), load(false)
{
}

reg_t mcontrol_t::tdata1_read(const processor_t *proc) const noexcept {
  reg_t v = 0;
  auto xlen = proc->get_xlen();
  v = set_field(v, MCONTROL_TYPE(xlen), type);
  v = set_field(v, MCONTROL_DMODE(xlen), dmode);
  v = set_field(v, MCONTROL_MASKMAX(xlen), maskmax);
  v = set_field(v, MCONTROL_SELECT, select);
  v = set_field(v, MCONTROL_TIMING, timing);
  v = set_field(v, MCONTROL_ACTION, action);
  v = set_field(v, MCONTROL_CHAIN, chain);
  v = set_field(v, MCONTROL_MATCH, match);
  v = set_field(v, MCONTROL_M, m);
  v = set_field(v, MCONTROL_H, h);
  v = set_field(v, MCONTROL_S, s);
  v = set_field(v, MCONTROL_U, u);
  v = set_field(v, MCONTROL_EXECUTE, execute);
  v = set_field(v, MCONTROL_STORE, store);
  v = set_field(v, MCONTROL_LOAD, load);
  return v;
}

bool mcontrol_t::tdata1_write(processor_t *proc, const reg_t val) noexcept {
  if (dmode && !proc->get_state()->debug_mode) {
    return false;
  }
  auto xlen = proc->get_xlen();
  dmode = get_field(val, MCONTROL_DMODE(xlen));
  select = get_field(val, MCONTROL_SELECT);
  timing = get_field(val, MCONTROL_TIMING);
  action = (triggers::action_t) get_field(val, MCONTROL_ACTION);
  chain = get_field(val, MCONTROL_CHAIN);
  match = (triggers::mcontrol_t::match_t) get_field(val, MCONTROL_MATCH);
  m = get_field(val, MCONTROL_M);
  h = get_field(val, MCONTROL_H);
  s = get_field(val, MCONTROL_S);
  u = get_field(val, MCONTROL_U);
  execute = get_field(val, MCONTROL_EXECUTE);
  store = get_field(val, MCONTROL_STORE);
  load = get_field(val, MCONTROL_LOAD);
  // Assume we're here because of csrw.
  if (execute)
    timing = 0;
  proc->trigger_updated();
  return true;
}

reg_t mcontrol_t::tdata2_read(const processor_t *proc) const noexcept {
  return tdata2;
}

bool mcontrol_t::tdata2_write(processor_t *proc, const reg_t val) noexcept {
  if (dmode && !proc->get_state()->debug_mode) {
    return false;
  }
  tdata2 = val;
  return true;
}

module_t::module_t(unsigned count) : triggers(count) {
  for (unsigned i = 0; i < count; i++) {
    triggers[i] = new mcontrol_t();
  }
}

// Return the index of a trigger that matched, or -1.
int module_t::trigger_match(triggers::operation_t operation, reg_t address, reg_t data)
{
  state_t *state = proc->get_state();
  if (state->debug_mode)
    return -1;

  bool chain_ok = true;
  auto xlen = proc->get_xlen();

  for (unsigned int i = 0; i < triggers.size(); i++) {
    if (!chain_ok) {
      chain_ok |= !triggers[i]->chain;
      continue;
    }

    if ((operation == triggers::OPERATION_EXECUTE && !triggers[i]->execute) ||
        (operation == triggers::OPERATION_STORE && !triggers[i]->store) ||
        (operation == triggers::OPERATION_LOAD && !triggers[i]->load) ||
        (state->prv == PRV_M && !triggers[i]->m) ||
        (state->prv == PRV_S && !triggers[i]->s) ||
        (state->prv == PRV_U && !triggers[i]->u)) {
      continue;
    }

    reg_t value;
    if (triggers[i]->select) {
      value = data;
    } else {
      value = address;
    }

    // We need this because in 32-bit mode sometimes the PC bits get sign
    // extended.
    if (xlen == 32) {
      value &= 0xffffffff;
    }

    auto tdata2 = triggers[i]->tdata2;
    switch (triggers[i]->match) {
      case triggers::mcontrol_t::MATCH_EQUAL:
        if (value != tdata2)
          continue;
        break;
      case triggers::mcontrol_t::MATCH_NAPOT:
        {
          reg_t mask = ~((1 << (cto(tdata2)+1)) - 1);
          if ((value & mask) != (tdata2 & mask))
            continue;
        }
        break;
      case triggers::mcontrol_t::MATCH_GE:
        if (value < tdata2)
          continue;
        break;
      case triggers::mcontrol_t::MATCH_LT:
        if (value >= tdata2)
          continue;
        break;
      case triggers::mcontrol_t::MATCH_MASK_LOW:
        {
          reg_t mask = tdata2 >> (xlen/2);
          if ((value & mask) != (tdata2 & mask))
            continue;
        }
        break;
      case triggers::mcontrol_t::MATCH_MASK_HIGH:
        {
          reg_t mask = tdata2 >> (xlen/2);
          if (((value >> (xlen/2)) & mask) != (tdata2 & mask))
            continue;
        }
        break;
    }

    if (!triggers[i]->chain) {
      return i;
    }
    chain_ok = true;
  }
  return -1;
}


};
