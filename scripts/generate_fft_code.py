from os import path
from numpy import cos, sin, pi, abs


def write_lines(file, ident, *lines):
    for line in lines:
        if line:
            file.write(" " * ident)
            file.write(line)
        file.write("\n")


def bit_reverse(normal, n_bits):
    reversed = 0
    for i, inv_i in zip(range(n_bits), range(n_bits - 1, -1, -1)):
        if normal & (1 << i):
            reversed += (1 << inv_i)
    return reversed


# Using some simple optimisations and fixed point arithmetic with this article as a basis:
# https://en.wikipedia.org/wiki/Cooley%E2%80%93Tukey_FFT_algorithm#Data_reordering,_bit_reversal,_and_in-place_algorithms
def write_fft(fft_file_path, log2_n_samples):
    n_samples = 1 << log2_n_samples

    precomputed_cos = [int(2**16 * cos(-2 * k * pi / n_samples)) for k in range(n_samples // 2)]
    precomputed_sin = [int(2**16 * sin(-2 * k * pi / n_samples)) for k in range(n_samples // 2)]

    with open(fft_file_path, "w") as f:
        ident_size = 4
        current_ident = 0

        write_lines(f,
                    current_ident,
                    "#include \"approx_mul.h\"",
                    "",
                    f"void approx_fft{n_samples}(int16_t *samples)",
                    "{")
        current_ident += ident_size

        # Get samples (bit reversed)
        for i in range(n_samples):
            write_lines(f, current_ident, f"int16_t re{i} = samples[{bit_reverse(i, log2_n_samples)}];")

        im_is_zero = [True for _ in range(n_samples)]

        for s in range(log2_n_samples):
            power = 1 << s
            for k in range(0, n_samples, 2 * power):
                for j in range(power):
                    block_lines = []

                    # t = exp[omega_index] * samples[k + j + power]
                    t_index = k + j + power
                    omega_idx = j * n_samples // (2 * power)
                    assert omega_idx < len(precomputed_cos)
                    re_t_is_zero = True
                    im_t_is_zero = True
                    re_t_val_name = f"re{t_index}"
                    im_t_val_name = "0" if im_is_zero[t_index] else f"im{t_index}"
                    cos_val = precomputed_cos[omega_idx]
                    sin_val = precomputed_sin[omega_idx]
                    if omega_idx == 0:
                        # angle is 0: cos is 1, sin is 0
                        # tre = samples_re[k + j + power]
                        # tim = samples_im[k + j + power]
                        block_lines.append(f"int16_t re_t = {re_t_val_name};")
                        re_t_is_zero = False
                        if not im_is_zero[t_index]:
                            block_lines.append(f"int16_t im_t = {im_t_val_name};")
                            im_t_is_zero = False
                    elif omega_idx == n_samples // 4:
                        # angle is -pi/2: cos is 0, sin is -1
                        # tre = samples_im[k + j + power]
                        # tim = - samples_re[k + j + power]
                        if not im_is_zero[t_index]:
                            block_lines.append(f"int16_t re_t = +{im_t_val_name};")
                            re_t_is_zero = False
                        block_lines.append(f"int16_t im_t = -{re_t_val_name};")
                        im_t_is_zero = False
                    elif omega_idx == n_samples // 8:
                        # angle is -pi/4: cos = -sin = sqrt(2)/2
                        # tre = cos[omega_index] * (samples_im[k + j + power] + samples_re[k + j + power])
                        # tim = cos[omega_index] * (samples_im[k + j + power] - samples_re[k + j + power])
                        block_lines.append(f"int16_t re_t = approx_mul_shift16({re_t_val_name} + {im_t_val_name}, {cos_val});")
                        block_lines.append(f"int16_t im_t = approx_mul_shift16({im_t_val_name} - {re_t_val_name}, {cos_val});")
                        re_t_is_zero = False
                        im_t_is_zero = False
                    elif omega_idx == 3 * (n_samples // 8):
                        # angle is -3*pi/4: -cos = -sin = sqrt(2)/2
                        # tre = cos[n_samples // 4] * ( - samples_re[k + j + power] + samples_im[k + j + power])
                        # tim = cos[n_samples // 4] * ( - samples_re[k + j + power] - samples_im[k + j + power])
                        block_lines.append(f"int16_t re_t = approx_mul_shift16(-{re_t_val_name} + {im_t_val_name}, {-cos_val});")
                        block_lines.append(f"int16_t im_t = approx_mul_shift16(-{im_t_val_name} - {re_t_val_name}, {-cos_val});")
                        re_t_is_zero = False
                        im_t_is_zero = False
                    else:
                        # no optimization
                        # tre = cos[omega_index] * samples_re[k + j + power] - sin[omega_index] * samples_im[k + j + power]
                        # tim = sin[omega_index] * samples_re[k + j + power] + cos[omega_index] * samples_im[k + j + power]
                        block_lines.append(f"int16_t re_t = {'+' if cos_val > 0 else '-'} approx_mul_shift16({re_t_val_name}, {abs(cos_val)}) {'-' if sin_val > 0 else '+'} {'0' if im_is_zero[t_index] else f'approx_mul_shift16({im_t_val_name}, {abs(sin_val)})'};")
                        block_lines.append(f"int16_t im_t = {'+' if sin_val > 0 else '-'} approx_mul_shift16({re_t_val_name}, {abs(sin_val)}) {'+' if cos_val > 0 else '-'} {'0' if im_is_zero[t_index] else f'approx_mul_shift16({im_t_val_name}, {abs(cos_val)})'};")
                        re_t_is_zero = False
                        im_t_is_zero = False

                    # u = samples[k + j]
                    # ure = samples_re[k + j]
                    # uim = samples_im[k + j]
                    u_index = k + j
                    block_lines.append(f"int16_t re_u = re{u_index};")
                    im_u_is_zero = im_is_zero[u_index]
                    if not im_u_is_zero:
                        block_lines.append(f"int16_t im_u = im{u_index};")

                    # samples[k + j] = u + t
                    # samples[k + j + power] = u - t
                    block_lines.append(f"re{u_index} = re_u + {'0' if re_t_is_zero else 're_t'};")
                    block_lines.append(f"re{t_index} = re_u - {'0' if re_t_is_zero else 're_t'};")
                    if not im_u_is_zero or not im_t_is_zero:
                        block_lines.append(f"im{u_index} = {'0' if im_u_is_zero else 'im_u'} + {'0' if im_t_is_zero else 'im_t'};")
                        if im_is_zero[u_index]:
                            # Declare imaginary before block
                            write_lines(f, current_ident, f"int16_t im{u_index};")
                            im_is_zero[u_index] = False
                        block_lines.append(f"im{t_index} = {'0' if im_u_is_zero else 'im_u'} - {'0' if im_t_is_zero else 'im_t'};")
                        if im_is_zero[t_index]:
                            # Declare imaginary before block
                            write_lines(f, current_ident, f"int16_t im{t_index};")
                            im_is_zero[t_index] = False

                    # Write the block to the file
                    write_lines(f, current_ident, "{")
                    current_ident += ident_size
                    write_lines(f, current_ident, *block_lines)
                    current_ident -= ident_size
                    write_lines(f, current_ident, "}")

        for i in range(n_samples // 2):
            write_lines(f,
                        current_ident,
                        f"samples[{2 * i}] = re{i};",
                        f"samples[{2 * i + 1}] = {'0' if im_is_zero[i] else f'im{i}'};")

        # End function
        current_ident -= ident_size
        write_lines(f, current_ident, "}")

if __name__ == "__main__":
    header_path = path.join(path.dirname(__file__), "..", "include", "unrolled_fft.h")
    write_fft(header_path, 6)
