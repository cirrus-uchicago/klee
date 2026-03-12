# /// script
# requires-python = ">=3.8"
# dependencies = [
#     "torch",
#     "numpy",
#     "scikit-learn",
# ]
# ///

import os
import torch
import torch.nn as nn
import numpy as np
from sklearn.preprocessing import StandardScaler

device = 'cuda' if torch.cuda.is_available() else 'cpu'

class PolicyFeedforward(nn.Module):

    def __init__(self, input_dim, hidden_dim):
        super().__init__()
        self.input_dim = input_dim
        self.hidden_dim = hidden_dim
        self.scaler = StandardScaler()
        self.net = nn.Sequential(
            nn.Linear(self.input_dim, self.hidden_dim),
            nn.ReLU(),
            nn.Linear(self.hidden_dim, self.hidden_dim),
            nn.ReLU(),
            nn.Linear(self.hidden_dim, 1)
        )

    def forward(self, x):
        return self.net(x)

    @staticmethod
    def load(path):
        # weights_only=False needed because the checkpoint contains a
        # sklearn StandardScaler object alongside the state_dict.
        if device == 'cuda':
            model_file = torch.load(path, weights_only=False)
        else:
            model_file = torch.load(path, map_location='cpu', weights_only=False)
        model = PolicyFeedforward(model_file['input_dim'], model_file['hidden_dim'])
        model.load_state_dict(model_file['state_dict'])
        model.scaler = model_file['scaler']
        return model

_model = None

def init_model(model_type, model_path):
    torch.manual_seed(1)
    np.random.seed(1)
    model_type = model_type.decode('ascii')
    model_path = model_path.decode('ascii')
    model_path = os.path.expandvars(os.path.expanduser(model_path))
    global _model
    assert model_type == 'feedforward'
    assert os.path.exists(model_path), f"Model not found: {model_path}"
    _model = PolicyFeedforward.load(model_path)
    _model.eval()

def predict(x, hidden):
    assert _model is not None
    x = _model.scaler.transform(x)
    x = torch.FloatTensor(x)
    res = _model(x).squeeze(1).tolist()
    return res, None
