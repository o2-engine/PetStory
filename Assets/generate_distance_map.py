import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image
import numpy as np
from scipy.ndimage import distance_transform_edt
import os

def generate_distance_map(input_path, output_path):
    img = Image.open(input_path).convert('RGBA')
    alpha = np.array(img)[:, :, 3]

    opaque_mask = alpha > 0

    dist = distance_transform_edt(opaque_mask)

    max_dist = dist.max()
    if max_dist > 0:
        dist_normalized = (dist / max_dist * 255).astype(np.uint8)
    else:
        dist_normalized = np.zeros_like(alpha, dtype=np.uint8)

    result = Image.fromarray(np.stack([dist_normalized] * 3 + [alpha], axis=-1), 'RGBA')

    result.save(output_path)
    return output_path

class App:
    def __init__(self, root):
        root.title('Distance Map Generator')
        root.geometry('420x150')
        root.resizable(False, False)

        frame = tk.Frame(root, padx=16, pady=16)
        frame.pack(fill='both', expand=True)

        tk.Label(frame, text='Source PNG:').grid(row=0, column=0, sticky='w')

        self.path_var = tk.StringVar()
        tk.Entry(frame, textvariable=self.path_var, width=36).grid(row=0, column=1, padx=(8, 4))
        tk.Button(frame, text='Browse...', command=self.browse).grid(row=0, column=2)

        tk.Button(frame, text='Generate Distance Map', command=self.generate,
                  width=30, height=2).grid(row=1, column=0, columnspan=3, pady=(20, 0))

    def browse(self):
        path = filedialog.askopenfilename(filetypes=[('PNG images', '*.png')])
        if path:
            self.path_var.set(path)

    def generate(self):
        path = self.path_var.get().strip()
        if not path or not os.path.isfile(path):
            messagebox.showerror('Error', 'Please select a valid PNG file.')
            return

        base, ext = os.path.splitext(path)
        output_path = filedialog.asksaveasfilename(
            initialdir=os.path.dirname(path),
            initialfile=os.path.basename(base) + '_distmap.png',
            defaultextension='.png',
            filetypes=[('PNG images', '*.png')]
        )
        if not output_path:
            return

        try:
            out = generate_distance_map(path, output_path)
            messagebox.showinfo('Done', f'Saved to:\n{out}')
        except Exception as e:
            messagebox.showerror('Error', str(e))

if __name__ == '__main__':
    root = tk.Tk()
    App(root)
    root.mainloop()
