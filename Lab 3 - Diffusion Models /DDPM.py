import torch
import torch.nn as nn
import torch.nn.functional as F
from ResUNet import ConditionalUnet
from utils import *

device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

class ConditionalDDPM(nn.Module):
    def __init__(self, dmconfig):
        super().__init__()
        self.dmconfig = dmconfig
        self.device = device  # Define device attribute here
        self.loss_fn = nn.MSELoss()
        self.network = ConditionalUnet(1, self.dmconfig.num_feat, self.dmconfig.num_classes)

    def scheduler(self, t_s):
        beta_1, beta_T, T = self.dmconfig.beta_1, self.dmconfig.beta_T, self.dmconfig.T
        # ==================================================== #
        # YOUR CODE HERE:
        #   Inputs:
        #       t_s: the input time steps, with shape (B,1). 
        #   Outputs:
        #       one dictionary containing the variance schedule
        #       $\beta_t$ along with other potentially useful constants.       
        beta_t = beta_1 + ((beta_T - beta_1) * (t_s - 1) / (T - 1))
        sqrt_beta_t = torch.sqrt(beta_t)
        alpha_t = 1 - beta_t
        oneover_sqrt_alpha = 1 / (torch.sqrt(alpha_t))
        alpha = 1 - torch.linspace(beta_1, beta_T, steps=T)
        alpha_t_bar_list = torch.cumprod(alpha, dim=0)
        t_s = t_s.to(alpha_t_bar_list.device)
        alpha_t_bar = alpha_t_bar_list[t_s.long() - 1]
        sqrt_alpha_bar = torch.sqrt(alpha_t_bar)
        sqrt_oneminus_alpha_bar = torch.sqrt(1 - alpha_t_bar)
        # ==================================================== #
        return {
            'beta_t': beta_t,
            'sqrt_beta_t': sqrt_beta_t,
            'alpha_t': alpha_t,
            'sqrt_alpha_bar': sqrt_alpha_bar,
            'oneover_sqrt_alpha': oneover_sqrt_alpha,
            'alpha_t_bar': alpha_t_bar,
            'sqrt_oneminus_alpha_bar': sqrt_oneminus_alpha_bar
        }

    def forward(self, images, conditions):
        T = self.dmconfig.T
        noise_loss = None
        # ==================================================== #
        # YOUR CODE HERE:
        #   Complete the training forward process based on the
        #   given training algorithm.
        #   Inputs:
        #       images: real images from the dataset, with size (B,1,28,28).
        #       conditions: condition labels, with size (B). You should
        #                   convert it to one-hot encoded labels with size (B,10)
        #                   before making it as the input of the denoising network.
        #   Outputs:
        #       noise_loss: loss computed by the self.loss_fn function  .  
        device = self.device  # Use self.device here
        B = images.shape[0]
        one_hot_cond = F.one_hot(conditions, num_classes=10)
        X_t = torch.randn_like(images, device=device)
        time_steps = torch.randint(1, T + 1, (B, 1, 1, 1), device=device)

        schedule = self.scheduler(time_steps)
        sqrt_alpha_bar = schedule['sqrt_alpha_bar'].to(device)
        sqrt_oneminus_alpha_bar = schedule['sqrt_oneminus_alpha_bar'].to(device)
        x_t = sqrt_alpha_bar * images + sqrt_oneminus_alpha_bar * X_t
        t = time_steps / T
        eps_pred = self.network.forward(x_t, t, one_hot_cond)
        noise_loss = self.loss_fn(eps_pred, X_t)
        # ==================================================== #
        
        return noise_loss

    def sample(self, conditions, omega):
        T = self.dmconfig.T
        X_t = None
        # ==================================================== #
        # YOUR CODE HERE:
        #   Complete the training forward process based on the
        #   given sampling algorithm.
        #   Inputs:
        #       conditions: condition labels, with size (B). You should
        #                   convert it to one-hot encoded labels with size (B,10)
        #                   before making it as the input of the denoising network.
        #       omega: conditional guidance weight.
        #   Outputs:
        #       generated_images  

        device = next(self.network.parameters()).device
        B = conditions.shape[0]
        h = self.dmconfig.input_dim[0]
        w = self.dmconfig.input_dim[1]
        c = self.dmconfig.num_channels
        condition_mask_value = self.dmconfig.condition_mask_value
        X_t = torch.randn(B, c, h, w, device=device)
        with torch.no_grad():
            for t in torch.arange(T, 0, -1):
                time_steps = torch.full((B, c, 1, 1), t, device=device)
                # sample unit variance guassian noise
                Z = torch.randn_like(X_t, device=device) if t > 1 else torch.zeros_like(X_t, device=device)
                schedule_dict = self.scheduler(t)
                beta_t = schedule_dict['beta_t'].to(device)
                alpha_t = schedule_dict['alpha_t'].to(device)
                oneover_sqrt_alpha = schedule_dict['oneover_sqrt_alpha'].to(device)
                sqrt_oneminus_alpha_bar = schedule_dict['sqrt_oneminus_alpha_bar'].to(device)
                sigma_t = torch.sqrt(beta_t)

                time_steps = (time_steps / T)
                cond_predict = self.network(X_t, time_steps, conditions)
                uncond_predict = self.network(X_t, time_steps, conditions * condition_mask_value)
                epsilon_t = (1 + omega) * cond_predict - omega * uncond_predict
                X_t = oneover_sqrt_alpha * (X_t - (1 - alpha_t) / sqrt_oneminus_alpha_bar * epsilon_t) + sigma_t * Z

            # ==================================================== #
            generated_images = (X_t * 0.3081 + 0.1307).clamp(0,1) # denormalize the output images
        return generated_images
