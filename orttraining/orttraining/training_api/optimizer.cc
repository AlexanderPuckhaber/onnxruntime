// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/providers/cpu/cpu_execution_provider.h"
#include "core/session/inference_session.h"
#include "core/session/environment.h"

#include "orttraining/training_api/include/utils.h"
#include "orttraining/training_api/include/optimizer.h"

namespace onnxruntime {
namespace training {
namespace api {

// TODO: don't hard code the state names, should get the state names according to the optimizer types.
// TODO: Conolidate with frontend tooling
const std::vector<std::string> MOMENT_SUFFIXES{".exp_avg", ".exp_avg_sq"};
const std::vector<std::string> MOMENT_STATE_NAMES{"momentum0", "momentum1"};

Status Optimizer::GenerateMomentumNamedStates() {
  auto& param_named_optimizer_states = optimizer_state_.param_named_optimizer_states;
  auto& optim_sess_state = optim_sess_->GetSessionState();
  for (auto& pair : named_parameters_) {
    if (pair.second->RequiresGrad()) {
      param_named_optimizer_states.insert({pair.first, ParameterOptimizerState()});
      ParameterOptimizerState& cur_param_optimizer_states = param_named_optimizer_states[pair.first];
      for (auto& state_name : MOMENT_STATE_NAMES) {
        OrtValue param_state;
        ORT_ENFORCE(utils::OrtValueLike(optim_sess_state, pair.second->Data(), param_state).IsOK(), "Error generating moment state for ", pair.first);
        cur_param_optimizer_states.momentum_named_states.insert({state_name, std::move(param_state)});
      }
    }
  }
  return Status::OK();
}

// Constructs the ortvalue inputs to be fed to the graph
// at each step
Status Optimizer::ConstructInputs() {
  if (optimizer_type_ == OptimizerType::AdamW) {
    auto& param_named_optimizer_states = optimizer_state_.param_named_optimizer_states;

    std::vector<Tensor> params, first_order_moments, second_order_moments;
    // TODO: Change to tensor seq implementation once clip grad norm op
    // that accepts tensor seq as input for gradients is complete.
    std::vector<OrtValue> grads;
    for (auto& named_parameter : named_parameters_) {
      params.emplace_back(std::move(*(named_parameter.second->Data().GetMutable<Tensor>())));
      first_order_moments.emplace_back(std::move(*(param_named_optimizer_states.at(named_parameter.first).momentum_named_states.at(MOMENT_STATE_NAMES[0]).GetMutable<Tensor>())));
      second_order_moments.emplace_back(std::move(*(param_named_optimizer_states.at(named_parameter.first).momentum_named_states.at(MOMENT_STATE_NAMES[1]).GetMutable<Tensor>())));
    }

    // Collect all the grad inputs
    for (size_t i = 5; i < input_names_.size(); i++) {
      std::string param_name;
      if (utils::GetParamNameFromGradient(input_names_[i], param_name)) {
        auto named_parameter_it = named_parameters_.find(param_name);
        ORT_ENFORCE(named_parameter_it != named_parameters_.end(), "Unknown param: ", param_name, " for field: ", input_names_[i]);
        grads.push_back(named_parameter_it->second->Gradient());
      } else {
        ORT_ENFORCE(false, "This is an invalid graph. Optimizer graph contains unknown user input:", input_names_[i]);
      }
    }

    auto input_inserter = [](auto& tensors, auto* inputs) {
      ORT_ENFORCE(!tensors.empty(), "Tensors cannot be empty while building a tensor sequence.");
      TensorSeq tensor_seq(tensors.front().DataType());
      tensor_seq.SetElements(std::move(tensors));
      inputs->emplace_back(std::move(OrtValue(&tensor_seq, DataTypeImpl::GetType<TensorSeq>(), DataTypeImpl::GetType<TensorSeq>()->GetDeleteFunc())));
    };

    input_inserter(params, &inputs_);
    input_inserter(first_order_moments, &inputs_);
    input_inserter(second_order_moments, &inputs_);

    // add the gradients as ortvalues to inputs
    for (auto& grad : grads) {
      inputs_.push_back(grad);
    }
  }
  // Add other optimizer reordering logic here
  return Status::OK();
}

Optimizer::Optimizer(const std::string& optim_path_or_bytes,
                     const std::unordered_map<std::string, std::shared_ptr<Parameter>>& parameters) : named_parameters_(parameters) {
  // TODO: share threadpool, env with module session
  const SessionOptions session_options;
  std::unique_ptr<Environment> env;
  ORT_ENFORCE(Environment::Create(nullptr, env) == Status::OK(), "Enviroment creation fails.");
  optim_sess_ = std::move(std::make_unique<InferenceSession>(session_options, *env));

  ORT_THROW_IF_ERROR(optim_sess_->Load(optim_path_or_bytes));
  ORT_THROW_IF_ERROR(optim_sess_->Initialize());

  utils::GetGraphInputOutputNames(optim_sess_, input_names_, output_names_);
  ORT_ENFORCE(input_names_[0] == "learning_rate");        // TODO: make this better
  ORT_ENFORCE(input_names_[1] == "step");                 // TODO: make this better
  ORT_ENFORCE(input_names_[2] == "params");               // TODO: make this better
  ORT_ENFORCE(input_names_[3] == "first_order_moments");  // TODO: make this better
  ORT_ENFORCE(input_names_[4] == "second_order_moments"); // TODO: make this better

  if (optimizer_type_ == OptimizerType::AdamW) {
    ORT_THROW_IF_ERROR(GenerateMomentumNamedStates());
  }
  ORT_THROW_IF_ERROR(ConstructInputs());
}

Status Optimizer::Step() {
  OrtValue learning_rate_input, step_input;
  utils::WarpInOrtValue<float>(optimizer_state_.learning_rate, &learning_rate_input);
  utils::WarpInOrtValue<int64_t>(optimizer_state_.step, &step_input);
  std::vector<OrtValue> feeds({learning_rate_input, step_input});
  feeds.insert(feeds.end(), inputs_.begin(), inputs_.end());

  std::vector<OrtValue> outputs;
  auto status = optim_sess_->Run(RunOptions(), input_names_, feeds, output_names_, &outputs);
  ORT_THROW_IF_ERROR(status);

  // extract step output and update
  if (utils::GetValue<int64_t>(outputs[0]) == 1LL)
    optimizer_state_.step++;

  return Status::OK();
}

Status Optimizer::GetStateDict(OptimizerCheckpointState& optimizer_checkpoint_state) {
  auto& grouped_optimizer_states = optimizer_checkpoint_state.group_named_optimizer_states;

  // Currently all parameters are in a single group, so we hardcode group0 here.
  // To support multiple groups, Optimizer constructor need accept informations for groupping.
  const std::string group_zero_name = "group0";
  grouped_optimizer_states.insert({group_zero_name, std::make_shared<GroupOptimizerState>(optimizer_state_)});

  // Pass the optimizer session data transfer manager for data copying when saving.
  // An alternative is, we can do copy at this stage.
  ORT_RETURN_IF_NOT(optim_sess_, "optimizer session not initialized");
  const DataTransferManager& sess_data_transfer_manager = optim_sess_->GetDataTransferManager();
  optimizer_checkpoint_state.optimizer_session_data_transfer_mgr = &sess_data_transfer_manager;
  return Status::OK();
}

}  // namespace api
}  // namespace training
}  // namespace onnxruntime
