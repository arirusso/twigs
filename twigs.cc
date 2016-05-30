//
// Twigs
// Alternate firmware for MI Branches
// Copyright 2016 Ari Russo
//
// Licensed GPL3.0
//
// Based on code from the original MI Branches firmware
// Copyright 2012 Olivier Gillet and licensed GPL 3.0
//

#include <avr/eeprom.h>

#include "avrlib/adc.h"
#include "avrlib/boot.h"
#include "avrlib/gpio.h"
#include "avrlib/watchdog_timer.h"

using namespace avrlib;

// Hardware
Gpio<PortD, 4> in_1;
Gpio<PortD, 3> out_1_a;
Gpio<PortD, 0> out_1_b;
Gpio<PortD, 1> led_1_a;
Gpio<PortD, 2> led_1_k;

Gpio<PortD, 7> in_2;
Gpio<PortD, 6> out_2_a;
Gpio<PortD, 5> out_2_b;
Gpio<PortB, 1> led_2_a;
Gpio<PortB, 0> led_2_k;

Gpio<PortC, 2> button_2;
Gpio<PortC, 3> button_1;

// Global
#define SYSTEM_NUM_CHANNELS 2
// Gate inputs
// Top input must be the reset function since the two inputs are hardware normaled
#define GATE_INPUT_RESET_INDEX 0
#define GATE_INPUT_TRIG_INDEX 1
// Buttons
#define BUTTON_LONG_PRESS_DURATION 9375 // 1200 * 8000 / 1024
// LEDs
#define LED_THRU_GATE_DURATION 0x100
#define LED_FACTORED_GATE_DURATION 0x080
// Pulse Tracker
#define PULSE_TRACKER_MAX_INSTANCES 2
#define PULSE_TRACKER_BUFFER_SIZE 2
#define PULSE_TRACKER_INPUT_INDEX 0
#define PULSE_TRACKER_CHAIN_INDEX 1
// ADC
#define ADC_DELTA_THRESHOLD 4 // ignore ADC updates less than this absolute value
#define ADC_MAX_VALUE 250
// amount of cycles between adc scans. higher number = better performace
#define ADC_POLL_RATIO 5 // 1:5
// Common function values
#define FUNCTION_TIMING_ERROR_CORRECTION_AMOUNT 12
// Swing
#define SWING_FACTOR_MIN 50
// Swing maximum amount can be adjusted up to 99
#define SWING_FACTOR_MAX 70
// Factorer
//
// The number 15 represents the set:
//  -8, -7, -6, -5, -4, -3, -2, 0, 2, 3, 4, 5, 6, 7, 8
//
// negative numbers are multiplier factors
// positive numbers are divider factors
// and zero is bypass
#define FACTORER_NUM_FACTORS 15
// The index of zero in the above set
// This is the control setting where factorer is neither dividing nor
// multiplying
#define FACTORER_BYPASS_INDEX 7

// Adc
AdcInputScanner adc;
uint8_t adc_counter;
int16_t adc_value[SYSTEM_NUM_CHANNELS];

// Gate input
bool gate_input_state[SYSTEM_NUM_CHANNELS];

// Buttons
bool button_state[SYSTEM_NUM_CHANNELS];
bool button_is_inhibited[SYSTEM_NUM_CHANNELS];
uint16_t button_last_press_at[SYSTEM_NUM_CHANNELS];

// LEDs
uint8_t led_state[SYSTEM_NUM_CHANNELS];
uint16_t led_gate_duration[SYSTEM_NUM_CHANNELS];

// Channel state
uint16_t channel_last_action_at[SYSTEM_NUM_CHANNELS];
uint8_t exec_state[SYSTEM_NUM_CHANNELS];

// Available functions
enum ChannelFunction {
  CHANNEL_FUNCTION_FACTORER,
  CHANNEL_FUNCTION_SWING,
  CHANNEL_FUNCTION_LAST
};
// Default functions
ChannelFunction channel_function_[SYSTEM_NUM_CHANNELS] = {
  CHANNEL_FUNCTION_SWING,
  CHANNEL_FUNCTION_FACTORER
};

// Common function vars
uint16_t pulse_tracker_buffer[PULSE_TRACKER_MAX_INSTANCES][PULSE_TRACKER_BUFFER_SIZE];
int16_t factor[SYSTEM_NUM_CHANNELS];

// Multiply
bool multiply_is_debouncing[SYSTEM_NUM_CHANNELS];

// Divide
int8_t divide_counter[SYSTEM_NUM_CHANNELS];

// Swing
int16_t swing[SYSTEM_NUM_CHANNELS];
int8_t swing_counter[SYSTEM_NUM_CHANNELS];

// Initialize the gate inputs (used for trig/reset)
void GateInputsInit() {
  in_1.set_mode(DIGITAL_INPUT);
  in_1.High();
  gate_input_state[0] = false;

  in_2.set_mode(DIGITAL_INPUT);
  in_2.High();
  gate_input_state[1] = false;
}

// Initialize the push buttons
void ButtonsInit() {
  button_1.set_mode(DIGITAL_INPUT);
  button_2.set_mode(DIGITAL_INPUT);
  button_1.High();
  button_2.High();

  button_state[0] = button_state[1] = false;
}

// Initialize the outputs
void GateOutputsInit() {
  out_1_a.set_mode(DIGITAL_OUTPUT);
  out_1_b.set_mode(DIGITAL_OUTPUT);
  out_2_a.set_mode(DIGITAL_OUTPUT);
  out_2_b.set_mode(DIGITAL_OUTPUT);
}

// Initialize the LEDs
void LedsInit() {
  led_1_a.set_mode(DIGITAL_OUTPUT);
  led_1_k.set_mode(DIGITAL_OUTPUT);

  led_2_a.set_mode(DIGITAL_OUTPUT);
  led_2_k.set_mode(DIGITAL_OUTPUT);

  led_1_a.Low();
  led_2_a.Low();
  led_1_k.Low();
  led_2_k.Low();

  led_state[0] = led_state[1] = 0;
}

// Initialize the pots and CV inputs
void AdcInit() {
  adc.Init();
  adc.set_num_inputs(SYSTEM_NUM_CHANNELS);
  Adc::set_reference(ADC_DEFAULT);
  Adc::set_alignment(ADC_LEFT_ALIGNED);
  adc_counter = 1;
}

// Load the stored system settings from the eeprom
// Currently, this consists of which functions are active on each channel
void SystemLoadState() {
  uint8_t configuration_byte = ~eeprom_read_byte((uint8_t*) 0);
  // bytes 1 2 4 8
  for (uint8_t i = 0; i < SYSTEM_NUM_CHANNELS; ++i) {
    uint8_t b = (i+1) * (i+1);
    if (configuration_byte & b) {
      channel_function_[i] = CHANNEL_FUNCTION_FACTORER;
    } else if (configuration_byte & (b * 2)) {
      channel_function_[i] = CHANNEL_FUNCTION_SWING;
    }
  }
}

// Initialize the system
void SystemInit() {
  Gpio<PortB, 4>::set_mode(DIGITAL_OUTPUT);
  Gpio<PortB, 4>::Low();

  // Hardware interface
  GateInputsInit();
  ButtonsInit();
  GateOutputsInit();
  LedsInit();
  AdcInit();

  SystemLoadState();

  TCCR1A = 0;
  TCCR1B = 5;

}

// Read the value of the given gate input
inline bool GateInputRead(uint8_t channel) {
  return channel == 0 ? !in_1.value() : !in_2.value();
}

// Read the value of the given button
bool ButtonRead(uint8_t channel) {
  return channel == 0 ? !button_1.value() : !button_2.value();
}

// Set the given output to high
inline void GateOutputOn(uint8_t channel) {
  switch (channel) {
    case 0: out_1_a.High();
            out_1_b.High();
            break;
    case 1: out_2_a.High();
            out_2_b.High();
            break;
  }
}

// Set the given output to low
inline void GateOutputOff(uint8_t channel) {
  switch (channel) {
    case 0: out_1_a.Low();
            out_1_b.Low();
            break;
    case 1: out_2_a.Low();
            out_2_b.Low();
            break;
  }
}

// Clear both of the values in the Pulse Tracker
inline void PulseTrackerClear(uint8_t id) {
  pulse_tracker_buffer[id][PULSE_TRACKER_BUFFER_SIZE-2] = 0;
  pulse_tracker_buffer[id][PULSE_TRACKER_BUFFER_SIZE-1] = 0;
}

// The amount of time since the last tracked event
inline uint16_t PulseTrackerGetElapsed(uint8_t id) {
  return TCNT1 - pulse_tracker_buffer[id][PULSE_TRACKER_BUFFER_SIZE - 1];
}

// The period of time between the last two recorded events
inline uint16_t PulseTrackerGetPeriod(uint8_t id) {
  return pulse_tracker_buffer[id][PULSE_TRACKER_BUFFER_SIZE - 1] - pulse_tracker_buffer[id][PULSE_TRACKER_BUFFER_SIZE - 2];
}

// Record the current time as the latest pulse tracker event and shift the last one back
void PulseTrackerRecord(uint8_t id) {
  // shift
  pulse_tracker_buffer[id][PULSE_TRACKER_BUFFER_SIZE - 2] = pulse_tracker_buffer[id][PULSE_TRACKER_BUFFER_SIZE - 1];
  pulse_tracker_buffer[id][PULSE_TRACKER_BUFFER_SIZE - 1] = TCNT1;
}

bool PulseTrackerIsPopulated(uint8_t id) {
  return pulse_tracker_buffer[id][PULSE_TRACKER_BUFFER_SIZE - 1] > 0 &&
    pulse_tracker_buffer[id][PULSE_TRACKER_BUFFER_SIZE - 2] > 0;
}

// Is the factor control setting such that we're in multiplier mode?
inline bool MultiplyIsEnabled(uint8_t channel) {
  return factor[channel] < 0;
}

// Is the pulse tracker populated with enough events to perform multiply?
inline bool MultiplyIsPossible(uint8_t channel) {
  return PulseTrackerIsPopulated(PULSE_TRACKER_INPUT_INDEX);
}

// The time interval between multiplied events
// eg if clock is comes in at 100 and 200, and the clock multiply factor is 2,
// the result will be 50
inline uint16_t MultiplyInterval(uint8_t channel) {
  return PulseTrackerGetPeriod(PULSE_TRACKER_INPUT_INDEX) / -factor[channel];
}

// Should the multiplier function exec on this cycle?
inline bool MultiplyShouldStrike(uint8_t channel, uint16_t elapsed) {
  uint16_t interval = MultiplyInterval(channel);
  if (elapsed % interval <= FUNCTION_TIMING_ERROR_CORRECTION_AMOUNT) {
    if (!multiply_is_debouncing[channel]) {
      return true;
    }
    // debounce window/return false
  } else {
    // debounce is finished
    multiply_is_debouncing[channel] = false;
  }
  return false;
}

// Is the factor setting such that we're in divider mode?
inline bool DivideIsEnabled(uint8_t channel) {
  return factor[channel] > 0;
}

// Should the divider function exec on this cycle?
inline bool DivideShouldStrike(uint8_t channel) {
  return divide_counter[channel] <= 0;
}

// What is the current factor setting?
inline int16_t FactorGet(uint8_t channel) {
  int16_t factor_index = (adc_value[channel] / (ADC_MAX_VALUE / (FACTORER_NUM_FACTORS - 1))) - FACTORER_BYPASS_INDEX;
  // offset result so that there's no -1 or 1 factor, but values are still evenly spaced
  if (factor_index == 0) {
    return 0;
  } else if (factor_index < 0) {
    return --factor_index;
  } else {
    return ++factor_index;
  }
}

// Turn off the LED for the given channel
inline void LedOff(uint8_t channel) {
  switch(channel) {
    case 0: led_1_a.Low();
            led_1_k.Low();
            break;
    case 1: led_2_a.Low();
            led_2_k.Low();
            break;
  }
}

// Make the LED for the given channel green
inline void LedGreen(uint8_t channel) {
  switch(channel) {
    case 0: led_1_a.Low();
            led_1_k.High();
            break;
    case 1: led_2_a.Low();
            led_2_k.High();
            break;
  }
}

// Make the LED for the given channel red
inline void LedRed(uint8_t channel) {
  switch(channel) {
    case 0: led_1_a.High();
            led_1_k.Low();
            break;
    case 1: led_2_a.High();
            led_2_k.Low();
            break;
  }
}

// Scan both pots and CV inputs for changes
inline void AdcScan() {
  if (adc_counter == (ADC_POLL_RATIO-1)) {
    adc.Scan();
    adc_counter = 0;
  } else {
    ++adc_counter;
  }
}

// The value for the pot/CV input for the given channel
inline int16_t AdcReadValue(uint8_t channel) {
  uint8_t pin = (channel == 0) ? 1 : 0;
  return adc.Read8(pin);
}

// Does the pot/CV input for the given channel have a new value since last checked?
bool AdcHasNewValue(uint8_t channel) {
  if (adc_counter == 0) {
    int16_t value = AdcReadValue(channel);
    // compare to stored control value
    int16_t delta = value - adc_value[channel];
    // abs
    if (delta < 0) {
      delta = -delta;
    }
    if (delta > ADC_DELTA_THRESHOLD) {
      // store control value
      adc_value[channel] = ADC_MAX_VALUE - value;
      // appears to be variance between channels, so limit the value
      if (adc_value[channel] < 0) {
        adc_value[channel] = 0;
      } else if (adc_value[channel] > ADC_MAX_VALUE) {
        adc_value[channel] = ADC_MAX_VALUE;
      }
      return true;
    }
  }
  return false;
}

// Has the internal clock reached overflow?
inline bool ClockIsOverflow() {
  return (channel_last_action_at[0] > TCNT1 || channel_last_action_at[1] > TCNT1);
}

// Clear the pulse tracker and other time based variables.  Due to clock overflow
// their meaning has been lost
inline void ClockHandleOverflow() {
  for (uint8_t i = 0; i < PULSE_TRACKER_MAX_INSTANCES; ++i) {
    PulseTrackerClear(i);
  }
  for (uint8_t i = 0; i < SYSTEM_NUM_CHANNELS; ++i) {
    channel_last_action_at[i] = 0;
    button_last_press_at[i] = 0;
    button_is_inhibited[i] = false;
  }
}

// For the given channel, use the LEDs to signify that trig thru is occurring
// EG in multiplier mode, an output that occurs at the same time as a trig input
inline void LedExecThru(uint8_t channel) {
  led_gate_duration[channel] = LED_THRU_GATE_DURATION;
  led_state[channel] = 1;
}

// For the given channel, use the LEDs to signify that a factored output is happening
// EG in multiplier mode, an output that occurs between trig inputs
inline void LedExecStrike(uint8_t channel) {
  led_gate_duration[channel] = LED_FACTORED_GATE_DURATION;
  led_state[channel] = 2;
}

// For the given channel, get the current swing amount value specified by the pot/CV input
inline int16_t SwingGet(uint8_t channel) {
  return (adc_value[channel] / (ADC_MAX_VALUE / (SWING_FACTOR_MAX - SWING_FACTOR_MIN))) + SWING_FACTOR_MIN;
}

// Update the LEDs for the given channel based on the current system state
inline void LedUpdate(uint8_t channel) {
  //
  if (led_gate_duration[channel]) {
    --led_gate_duration[channel];
    if (!led_gate_duration[channel]) {
      led_state[channel] = 0;
    }
  }

  // Update Leds
  switch (led_state[channel]) {
    case 0: LedOff(channel);
            break;
    case 1: LedGreen(channel);
            break;
    case 2: LedRed(channel);
            break;
  }
}

inline bool FactorerHasChainedSwing(uint8_t channel) {
  uint8_t other_channel = (channel == 0) ? 1 : 0;
  return (channel_function_[other_channel] == CHANNEL_FUNCTION_SWING);
}

// For the given channel, pulses stored in the pulse tracker, and the factor setting
// of the swing function, what is the time interval that the swung output will be delayed
// passed the corresponding input gate?
//
// IE in the following illustration of a full swing routine, the interval between "input pulse 2"
// and "swing strike"
//
// [input pulse1/swing thru].......[input pulse2]....[swing strike]..........
//
inline uint16_t SwingInterval(uint8_t channel) {
  uint16_t period = PulseTrackerGetPeriod(PULSE_TRACKER_INPUT_INDEX);
  return ((10 * (period * 2)) / (1000 / swing[channel])) - period;
}

// For the given amount of time since the last swing strike/thru, should the swing function
// on the given channel exec during this cycle?
inline bool SwingShouldStrike(uint8_t channel, uint16_t elapsed) {
  if (swing_counter[channel] >= 2 && swing[channel] > SWING_FACTOR_MIN) {
    uint16_t interval = SwingInterval(channel);
    return (elapsed >= interval &&
      elapsed <= interval + FUNCTION_TIMING_ERROR_CORRECTION_AMOUNT);
  } else {
    // thru
    return false;
  }
}

// Reset the swing function for the given channel
inline void SwingReset(uint8_t channel) {
  swing_counter[channel] = 0;
}

// Update the given channel's state to reflect a swing thru execution for this cycle
inline void SwingExecThru(uint8_t channel) {
  exec_state[channel] = 1;
  channel_last_action_at[channel] = TCNT1;
}

// Update the given channel's state to reflect a swing strike execution for this cycle
inline void SwingExecStrike(uint8_t channel) {
  exec_state[channel] = 2;
  channel_last_action_at[channel] = TCNT1;
}

void SwingHandleInput(uint8_t output_channel) {
  switch (swing_counter[output_channel]) {
    case 0: // thru beat
            SwingExecThru(output_channel);
            swing_counter[output_channel] = 1;
            break;
    case 1: // skipped thru beat
            // unless lowest setting, no swing - should do thru
            if (swing[output_channel] <= SWING_FACTOR_MIN) {
              SwingExecStrike(output_channel);
              SwingReset(output_channel);
            } else {
              // rest
              exec_state[output_channel] = 0;
              swing_counter[output_channel] = 2;
            }
            break;
    default: SwingReset(output_channel); // something is wrong if we're here so reset
             break;
  }
}

inline void FactorerExecChainedSwing(uint8_t channel) {
  PulseTrackerRecord(PULSE_TRACKER_CHAIN_INDEX);
  SwingHandleInput(channel);
}

// Is the gate input for the given channel seeing a new pulse?
inline bool GateInputIsRisingEdge(uint8_t channel) {
  bool last_state = gate_input_state[channel];
  // store current input state
  gate_input_state[channel] = GateInputRead(channel);
  //
  return gate_input_state[channel] && !last_state;
}

// For the given channel, update state for a multiply strike
inline void MultiplyExecStrike(uint8_t channel) {
  channel_last_action_at[channel] = TCNT1;
  exec_state[channel] = 2;
  multiply_is_debouncing[channel] = true;
  if (FactorerHasChainedSwing(channel)) {
    FactorerExecChainedSwing(channel);
  }
}

// For the given channel and current system state, execute a single
// cycle of the multiplier function
inline void MultiplyExec(uint8_t channel) {
  if (MultiplyIsEnabled(channel) &&
        MultiplyIsPossible(channel) &&
        MultiplyShouldStrike(channel, PulseTrackerGetElapsed(PULSE_TRACKER_INPUT_INDEX))) {
    MultiplyExecStrike(channel);
  }
}

// Update the given channel's state to reflect that the multiplier is executing
// thru for this cycle
inline void MultiplyExecThru(uint8_t channel) {
  exec_state[channel] = 1;
  channel_last_action_at[channel] = TCNT1;
  if (FactorerHasChainedSwing(channel)) {
    FactorerExecChainedSwing(channel);
  }
}

// For the given channel, reset the divider function
inline void DivideReset(uint8_t channel) {
  divide_counter[channel] = 0;
}

// For the given channel and current system state, should the divider function reset?
inline bool DivideShouldReset(uint8_t channel) {
  return divide_counter[channel] >= (factor[channel] - 1);
}

// For the given channel, update state for a divide strike
inline void DivideExecStrike(uint8_t channel) {
  channel_last_action_at[channel] = TCNT1;
  exec_state[channel] = 2; // divide converts thru to exec on every division
  if (FactorerHasChainedSwing(channel)) {
    FactorerExecChainedSwing(channel);
  }
}

// For the given channel, process a new pulse using the factorer function
void FactorerHandleInputGateRisingEdge(uint8_t channel) {
  //
  if (DivideIsEnabled(channel)) {
    if (DivideShouldStrike(channel)) {
      DivideExecStrike(channel);
    }
    // deal with counter
    if (DivideShouldReset(channel)) {
      DivideReset(channel);
    } else {
      ++divide_counter[channel];
    }
  } else {
    MultiplyExecThru(channel); // multiply always acknowledges thru
  }
}

// For the given channel, process a new pulse using the swing function
void SwingHandleInputGateRisingEdge(uint8_t channel) {
  SwingHandleInput(channel);
}

// For the given channel and current system state, execute a single
// cycle of the swing function
inline void SwingExec(uint8_t channel) {
  if (SwingShouldStrike(channel, PulseTrackerGetElapsed(PULSE_TRACKER_INPUT_INDEX))) {
    SwingExecStrike(channel);
    SwingReset(channel); // reset
  }
}

// For the given channel, handle a new value at the pot/CV input
inline void FunctionHandleNewAdcValue(uint8_t channel) {
  switch(channel_function_[channel]) {
    case CHANNEL_FUNCTION_FACTORER: factor[channel] = FactorGet(channel);
                                    break;
    case CHANNEL_FUNCTION_SWING: swing[channel] = SwingGet(channel);
                                 break;
  }
}

// For the given channel's function, execute a single cycle
inline void FunctionExec(uint8_t channel) {
  switch(channel_function_[channel]) {
    case CHANNEL_FUNCTION_FACTORER: MultiplyExec(channel);
                                    break;
    case CHANNEL_FUNCTION_SWING: SwingExec(channel);
                                 break;
  }

  // Do stuff
  if (exec_state[channel] > 0) {
    GateOutputOn(channel);
    (exec_state[channel] < 2) ? LedExecThru(channel) : LedExecStrike(channel);
  } else {
    GateOutputOff(channel);
  }
  exec_state[channel] = 0; // clean up
}

// Reset the given channel's function
inline void FunctionReset(uint8_t channel) {
  switch(channel_function_[channel]) {
    case CHANNEL_FUNCTION_FACTORER: DivideReset(channel);
                                    break;
    case CHANNEL_FUNCTION_SWING: SwingReset(channel);
                                 break;
  }
}

// For the given channel's function, handle a new input gate
inline void FunctionHandleInputGateRisingEdge(uint8_t channel) {
  switch(channel_function_[channel]) {
    case CHANNEL_FUNCTION_FACTORER: FactorerHandleInputGateRisingEdge(channel);
                                    break;
    case CHANNEL_FUNCTION_SWING: SwingHandleInputGateRisingEdge(channel);
                                    break;
  }
}

// Based on the given channel's state, execute a single system cycle
inline void ChannelExec(uint8_t channel) {
  // do stuff
  FunctionExec(channel);
  LedUpdate(channel);
}

// Update the given channel's state according to the system input state
inline void ChannelStateUpdate(uint8_t channel, bool is_trig, bool is_reset) {
  // Update for pot/cv in
  if (AdcHasNewValue(channel)) {
    FunctionHandleNewAdcValue(channel);
  }
  // Update for clock/trig/gate input
  if (is_trig) {
    FunctionHandleInputGateRisingEdge(channel);
  }
  // Update for reset
  if (is_reset) {
    FunctionReset(channel);
  }
}

// Save the system state to the eeprom
// Currently stores which functions are selected by the user
void SystemStateSave() {
  uint8_t configuration_byte = 0;
  // bytes 1 2 4 8
  for (uint8_t i = 0; i < SYSTEM_NUM_CHANNELS; ++i) {
    uint8_t b = (i+1) * (i+1);
    switch(channel_function_[i]) {
      case CHANNEL_FUNCTION_FACTORER: configuration_byte |= b;
                                      break;
      case CHANNEL_FUNCTION_SWING: configuration_byte |= (b * 2);
                                   break;
    }
  }
  eeprom_write_byte((uint8_t*) 0, ~configuration_byte);
}

// Toggle the function for the given channel
void ChannelFunctionToggle(uint8_t channel) {
  switch(channel_function_[channel]) {
    case CHANNEL_FUNCTION_FACTORER: channel_function_[channel] = CHANNEL_FUNCTION_SWING;
                                    break;
    case CHANNEL_FUNCTION_SWING: channel_function_[channel] = CHANNEL_FUNCTION_FACTORER;
                                 break;
  }
  FunctionReset(channel);
}

// For the given channel, record a button press start
inline void ButtonHandleNewlyPressed(uint8_t channel) {
  button_last_press_at[channel] = TCNT1;
  button_is_inhibited[channel] = false;
}

// For the given channel, is the button in a new state than it was last cycle?
inline bool ButtonIsNewState(uint8_t channel) {
  bool input_state = ButtonRead(channel);
  if (!button_state[channel] && input_state) {
    ButtonHandleNewlyPressed(channel);
  }
  return input_state;
}

// Scan the button state and execute any actions accordingly
void ButtonsScanAndExec() {
  for (uint8_t i = 0; i < SYSTEM_NUM_CHANNELS; ++i) {
    bool new_input_state = ButtonIsNewState(i);
    if (button_state[i] && !button_is_inhibited[i]) {
      uint16_t button_press_time = TCNT1 - button_last_press_at[i];
      if (button_press_time >= BUTTON_LONG_PRESS_DURATION) {
        button_is_inhibited[i] = true;
        // long press
        // toggle functions & save
        ChannelFunctionToggle(i);
        SystemStateSave();
      } else if (new_input_state) {
        // short press
        // do reset
        FunctionReset(i);
      }
    }
    button_state[i] = new_input_state;
  }
}

// Single system loop
inline void Loop() {

  if (ClockIsOverflow()) {
    ClockHandleOverflow();
  }

  // Scan pot/cv in
  AdcScan();

  // Scan buttons
  ButtonsScanAndExec();

  // Scan clock/trig/gate input
  bool is_trig = false;

  if (GateInputIsRisingEdge(GATE_INPUT_TRIG_INDEX)) {
    // Input pulse tracker is always recording. this should help smooth transitions
    // between functions even though divide doesn't use it
    PulseTrackerRecord(PULSE_TRACKER_INPUT_INDEX);
    is_trig = true;
  }

  // scan reset input
  bool is_reset = GateInputIsRisingEdge(GATE_INPUT_RESET_INDEX);

  // do stuff
  for (uint8_t i = 0; i < SYSTEM_NUM_CHANNELS; ++i) {
    ChannelStateUpdate(i, is_trig, is_reset);
    ChannelExec(i);
  }
}

int main(void) {
  ResetWatchdog();
  SystemInit();

  while (1) {
    Loop();
  }
}
