import dash
import os
import subprocess
import json
import jinja2
import sqlite3

import pandas                    as pd
import plotly.express            as px
import dash_bootstrap_components as dbc
import numpy                     as np
import plotly.graph_objs as go

import subprocess


from plotly.subplots import make_subplots
from dash    import html
from dash    import dcc
from dash    import callback
from dash    import Input
from dash    import Output
from glob    import glob
from dash    import ALL
from time    import sleep
from pathlib import Path

from synopsis import synopsis

# IMPORTANT:
import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)
# The above is Turning off the float warning spam

downlinked_dir = "downlink/"
onboard_dir = "onboard/"
onboard_color = '#d9ebf0'
ground_color = '#dbfcd9'
database_file = 'owls_asdpdb_20230815.db'
default_alpha = 0.5
default_data_volume = 2.4
mode="NOS3"

# This is just here for lookup w/o the database (makes a the code brittle)
file_dict = {'asdp000000000': 1.306, 'asdp000000001': 0.773, 'asdp000000002': 0.538, 'asdp000000003': 1.055, 'asdp000000004': 0.403, 'asdp000000005': 0.635, 'asdp000000006': 1.240, 'asdp000000007': 0.494 }

x_coord_dd_ind = 1
y_coord_dd_ind = 7

# Extrema applied by default to DD properties to force them to interval [0, 1]. Need to undo these when scaling
# If weighting is applied (it isn't by default), need to also undo that
dd_extrema = np.array([1000.,
                       1000.,
                       1000.,
                       500.,
                       500.,
                       500.,
                       20.,
                       20.,
                       20.])[[x_coord_dd_ind, y_coord_dd_ind]]

dd_names = ["bbox_area_min_pc10.0",
            "bbox_area_min_pc50.0",
            "bbox_area_min_pc90.0",
            "disp_e2e_norm_pc10.0",
            "disp_e2e_norm_pc50.0",
            "disp_e2e_norm_pc90.0",
            "speed_mean_pc10.0",
            "speed_mean_pc50.0",
            "speed_mean_pc90.0"]

dd_readable_names = ['Particle Size\n(Pixels)', # 10th percentile
                     'Particle Size\n(Pixels)', # 50th percentile
                     'Particle Size\n(Pixels)', # 90th percentile
                     'Mean Displacement\n(Pixels)', # 10th percentile
                     'Mean Displacement\n(Pixels)', # 50th percentile 
                     'Mean Displacement\n(Pixels)', # 90th percentile 
                     'Mean Speed\n(Pixels/Frame)',  # 10th percentile 
                     'Mean Speed\n(Pixels/Frame)',  # 50th percentile 
                     'Mean Speed\n(Pixels/Frame)']  # 90th percentile


def get_sues_dds(glob_path):
    experiments = glob(glob_path + "*/")
    sues, dds = [], []

    for experiment in experiments:
        sue_loc = glob(os.path.join(experiment,"asdp/*_sue.csv"))[0]
        sue_df = pd.read_csv(sue_loc)
        dd_df = pd.read_csv(glob(os.path.join(experiment,"asdp/*_dd.csv"))[0])

        sues.append(float(sue_df['SUE'][0]))
        dds.append([float(dd_df[dd_names[x_coord_dd_ind]]) * dd_extrema[0],
                    float(dd_df[dd_names[y_coord_dd_ind]]) * dd_extrema[1]])

    sues = np.array(sues)
    dds = np.array(dds)
    return sues, dds

def clear_folder(folder_path):

    print(f"Deleting contents of {folder_path}")
    experiments = glob(os.path.join(folder_path,"*")+"/")
    for experiment in experiments:
        subprocess.call(["rm", "-rf", experiment])

def copy_folder(src_path, dst_path):

    print(f"Copying contents of {src_path} to {dst_path}")
    experiments = glob(os.path.join(src_path,"*")+"/")
    for experiment in experiments:
        destination_name = dst_path.split("/")[0]
        subprocess.call(["cp", "-r", experiment, destination_name])


# def synopsis(path_to_synopsis_cli, asdpdb_file, rule_config_file, similarity_config_file, output_file=None):
#     '''
#     Entry point to C++ implementation of SYNOPSIS (synopsis_cli)

#     downlink states (given in dictionaries describing each DP):
#     UNTRANSMITTED = 0
#     TRANSMITTED = 1
#     DOWNLINKED = 2

#     Parameters
#     ----------
#     path_to_synopsis_cli: str
#         Path to the CLI executable `synopsis_cli`
#     asdpdb_file: str
#         Path to DB file
#     rule_config_file: str
#         Path to JSON file with rules
#     similarity_config_file: str
#         Path to JSON file with similarity and diversity parameters
#     output_file: str
#         Path to JSON file to output, or None if writing to file is not desired

#     Returns
#     -------
#     logs: list
#         List of log statements from SYNOPSIS
#     prioritized_dps: list
#         None if there is an error running SYNOPSIS, else a list of dictionaries holding DP information in order of priority 
    
    

#     '''

#     if not os.path.exists(asdpdb_file):
#         print("invalid asdpdb_file")
#     if not os.path.exists(rule_config_file):
#         print("invalid rule_config_file")
#     if not os.path.exists(similarity_config_file):
#         print("invalid similarity_config_file")

#     logs = None
#     prioritized_dps = None

#     try:
#         if output_file is not None:
#             print("output file given: ", output_file)
#             cp = subprocess.run(
#                 [path_to_synopsis_cli, asdpdb_file, rule_config_file, similarity_config_file, output_file],
#                 capture_output=True
#             )
#         else:
#             print("no output file given")
#             cp = subprocess.run(
#                 [path_to_synopsis_cli, asdpdb_file, rule_config_file, similarity_config_file],
#                 capture_output=True
#             )

#     except:
#         print("Error running SYNOPSIS, check synopsis_cli.cpp")

#     else:
#         if cp.returncode == 0:
#             print("SYNOPSIS ran successfully")
#         else:
#             print("SYNOPSIS returned error")

#         stddout = cp.stdout.decode().split('\n')
#         stderr = cp.stderr.decode().split('\n')

#         logs = []
#         msg = []
#         for line in stddout:
#             if "[INFO]" in line or "[WARN]" in line or "[ERROR]" in line:
#                 logs.append(line)
#             else:
#                 msg.append(line)
#         logs.extend(stderr)
        
#         msg = (' '.join(msg))
#         msg = json.loads(msg)
#         prioritized_dps = [msg[str(i)] for i in msg['prioritized_list']]

#     finally:
#         return logs, prioritized_dps




app = dash.Dash(title='SYNOPSIS Science Deno', external_stylesheets=[dbc.themes.BOOTSTRAP])
cnx = sqlite3.connect(database_file)
df = pd.read_sql_query("SELECT * FROM ASDP", cnx)

experiments = [x.split(".")[-2] for x in df.uri]
asdpdb = dict(zip(df.asdp_id, experiments))

app.layout = html.Div(id = 'parent', children = [
    html.H1("SYNOPSIS Science Demo", style={'textAlign':'center'}),
    html.P(html.I(html.Center("Science Yield improvemeNt via Onboard Prioritization and Summary of Information System"))),
    html.Br(),
    html.Div(id='controls2', style={'textAlign':'left', 'margin-left':20, 'margin-right':20}, children=[

        dbc.Row([
            dbc.Col([
                html.Br(),
                html.P([
                        "This interface allows for exploration of the ",
                        html.A("SYNOPSIS", href = "https://ml.jpl.nasa.gov/projects/synopsis/synopsis.html"),
                        " data prioritization mechanisms, with data from the ",
                        html.A("OWLS JNEXT Program", href="https://ml.jpl.nasa.gov/projects/owls/owls.html"),
                        ".  Use the slider bars to the right to adjust SYNOPSIS data prioritization parameters. ",
                        "The alpha parameter controls the preferred amount of diversity in downlinked data.  ",
                        "A high alpha will provide diverse data, while a low alpha will provide more similar data.  ",
                        "Adjust the data volume slider to control how much data can be downlinked from the spacecraft.  ",
                        "Data in the left column represents data remaining onboard the spacecraft, while data in the right column represents data which has been downlinked.  ",
                        "If you wish to see the data left onboard the spacecraft, select 'Show Onboard Data' from the dropdown near the bottom of the interface.",
                        ]),

            ]),
            dbc.Col([
                dbc.Row([
                    dbc.Col([
                        html.Div(id='controls', style={'textAlign':'center', 'margin-left':10, 'margin-right':10}),
                    ]),
                ]),
            ]),
        ]),
    ]),

    dbc.Row([

            # Onboard tiles
            dbc.Col(children=[
                dbc.Row(html.H3("Onboard Data", style={'textAlign': 'center'})),

                dbc.Row(children=[
                    dbc.Col(id="onboard_1"),
                    dbc.Col(id="onboard_2"),
                    dbc.Col(id="onboard_3"),
                    dbc.Col(id="onboard_4"),
                ], style={'padding': 10}),

                dbc.Row(children=[
                    dbc.Col(id="onboard_5"),
                    dbc.Col(id="onboard_6"),
                    dbc.Col(id="onboard_7"),
                    dbc.Col(id="onboard_8"),
                ], style={'padding': 10}),
                
            ], style={'padding': 10, 'flex': 1,'flex-direction': 'col','background-color': onboard_color}),

            # Downlink tiles
            dbc.Col(children=[
                dbc.Row(html.H3("Downlinked Data", style={'textAlign': 'center'})),

                dbc.Row(children=[
                    dbc.Col(id="downlinked_1"),
                    dbc.Col(id="downlinked_2"),
                    dbc.Col(id="downlinked_3"),
                    dbc.Col(id="downlinked_4"),
                ], style={'padding': 10}),

                dbc.Row(children=[
                    dbc.Col(id="downlinked_5"),
                    dbc.Col(id="downlinked_6"),
                    dbc.Col(id="downlinked_7"),
                    dbc.Col(id="downlinked_8"),
                ], style={'padding': 10}),

            ], style={'padding': 10, 'flex': 1,'flex-direction': 'col', 'background-color': ground_color}),

    ]),


    dbc.Row([

            # Onboard graph
            dbc.Col(children=[

                dbc.Row(children=[
                    dcc.Graph(id='onboard-plot'),
                ], style={'padding': 10, 'flex': 1,'flex-direction': 'col','background-color': onboard_color}),

            ], style={'padding': 10, 'flex': 1,'flex-direction': 'col','background-color': onboard_color}),

            # Downlink graph
            dbc.Col(children=[

                dbc.Col(children=[
                    dcc.Graph(id='downlinked-plot'),
                ], style={'padding': 10, 'flex': 1,'flex-direction': 'col', 'background-color': ground_color}),


            ], style={'padding': 10, 'flex': 1,'flex-direction': 'col', 'background-color': ground_color}),

    ]),

    dcc.Dropdown(
       options=['NOS3', 'Science Slider'],
       value='Science Slider',
       id='mode',
       style={'display':'none'}
    ),

    dcc.Dropdown(
        options=['Hide Onboard Data', 'Show Onboard Data'],
       value='Hide Onboard Data',
       id='hide-onboard'
    ),

    html.P(id='placeholder'),

])

@app.callback(
    [Output('controls', 'children')],
    [Input('mode', 'value')])
def update_controls(mode):

    controls = []

    if mode == "NOS3":
        pass
    controls.append(dbc.Row(html.H3("Alpha", style={'textAlign': 'center'})))
    controls.append(dcc.Slider(0, 1.00, 0.01, value=1.00, marks={0:'0',1:'1'}, tooltip={"placement": "bottom", "always_visible": True}, id={"type":'control', "index":1}))
    controls.append(dbc.Row(html.P("Adjust the comparison ALPHA values")))
    controls.append(html.Br())
    controls.append(dbc.Row(html.H3("Downlink Data Volume (Mb)", style={'textAlign': 'center'})))
    controls.append(dcc.Slider(0, 15.0, 0.1, value=5.0, marks={0:'0',15:'15'}, tooltip={"placement": "bottom", "always_visible": True}, id={"type":'control', "index":1}))
    controls.append(dbc.Row(html.P("Total available amount capable of downloading")))
    controls.append(html.Br())
    return [controls]

@app.callback(
    [Output('onboard-plot', 'figure'),
     Output('downlinked-plot', 'figure'),
     Output('onboard_1', 'children'),
     Output('onboard_2', 'children'),
     Output('onboard_3', 'children'),
     Output('onboard_4', 'children'),
     Output('onboard_5', 'children'),
     Output('onboard_6', 'children'),
     Output('onboard_7', 'children'),
     Output('onboard_8', 'children'),
     Output('downlinked_1', 'children'),
     Output('downlinked_2', 'children'),
     Output('downlinked_3', 'children'),
     Output('downlinked_4', 'children'),
     Output('downlinked_5', 'children'),
     Output('downlinked_6', 'children'),
     Output('downlinked_7', 'children'),
     Output('downlinked_8', 'children')],
    [Input({"type": "control", "index": ALL}, "value"),
     Input({"type": "control", "index": ALL}, "n_clicks"),
     Input('mode', 'value'),
     Input('hide-onboard', 'value'),
     Input('placeholder', 'children')],
     prevent_initial_call=True)
def update_output(control_values, control_clicks, mode, hide_onboard, _):

    if mode == "NOS3":
        pass
    
    try:
        alpha = control_values[0] 
        data_volume = control_values[1]
    except:
        alpha = 0.42
        data_volume = 5.0

    print(f'Slider used. Alpha={alpha}, Data Volume={data_volume}')

    dv_ = 0
    transfer = []

    template_loader = jinja2.FileSystemLoader('./')
    template_env = jinja2.Environment(loader=template_loader)

    html_template = 'owls_similarity_config.template'
    template = template_env.get_template(html_template)
    output_text = template.render({'alpha':alpha})

    text_file = open('/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/jpl_science/owls_similarity_config.json', "w")
    text_file.write(output_text)
    text_file.close()
                           
    path_to_synopsis_cli = '/home/nos3/Desktop/github-nos3/fsw/build/cpu1/default_cpu1/apps/syn_app/synopsis/synopsis_cli'
    asdpdb_file = '/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/jpl_science/owls_asdpdb_20230815.db'
    rule_config_file = '/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/jpl_science/empty_rules.json' # This may need to be changed to a populated rules file
    similarity_config_file = '/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/jpl_science/owls_similarity_config.json'

    logs, prioritized_dps = synopsis(path_to_synopsis_cli, asdpdb_file, rule_config_file, similarity_config_file)

    clear_folder("downlink/")
    clear_folder("onboard/")
    copy_folder("assets/", "onboard/")

    print("logs: ")
    for line in logs:
       print(line)

    print("prioritized dps: ")
    for i in prioritized_dps:
        #print(i)
        exp = asdpdb[i['dp_id']]
        exp_size = i['dp_size'] / (1024 * 1024)

        dv_ += exp_size

        if dv_ >= data_volume:
            continue
        else:
            transfer.append(exp)
            src_exp = os.path.join(onboard_dir,exp)
            dst_exp = os.path.join(downlinked_dir,exp)
            print(f"\nMoving {src_exp} to downlink/{exp}\n")
            subprocess.call(["mv", src_exp, "downlink/"])
            sleep(0.5)

    downlinks = glob("downlink/*/")
    onboards = glob("onboard/*/")

    cl = []

    # Fill onboard
    for x in range(0,8):

        try:

            observation = (onboards[x]).split("/")[1]
            obs_path = "assets/" + observation
            os.chdir("/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/jpl_science/")
            cwd = os.getcwd()
            
            if hide_onboard == "Hide Onboard Data":
                mhi_file = glob(os.path.join(obs_path, "validate/unknown.jpg"))
            else:
                mhi_file = glob(os.path.join(obs_path, "validate/*_mhi.jpg"))
            sue_file = glob(os.path.join(obs_path, "asdp/*_sue.csv"))[0]
            with open(sue_file) as file:
                sue = float([line.rstrip() for line in file][-1])
            file_size = file_dict[observation]
            tmp = dbc.Card([
                    dbc.CardHeader(f"{observation}"),
                    dbc.CardImg(src=mhi_file, top=True),
                    dbc.CardBody([
                        html.P(f"SUE = {sue:.2f}"),
                        html.P(f"Size = {file_size:.2f} Mb"),
                        #html.P(f"DQE = {dqe:.2f}")
                    ], style={'fontSize': 14,
                              'textAlign': 'center'}
                ),
            ],color="primary", outline=True)

        except:
            tmp = html.P()

        cl.append(tmp)

    # Fill downlinked
    for x in range(0,8):

        try:

            observation = (downlinks[x]).split("/")[1]

            obs_path = "assets/" + observation
            os.chdir("/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/jpl_science/")

            mhi_file = glob(obs_path + "/validate/*_mhi.jpg")
            sue_file = glob(os.path.join(obs_path, "asdp/*_sue.csv"))[0]
            with open(sue_file) as file:
                sue = float([line.rstrip() for line in file][-1])

            # dqe_file = glob(os.path.join(observation, "asdp/*_dqe.csv"))[0]
            # with open(dqe_file) as file:
            #     dqe =float([line.rstrip() for line in file][-1])
            file_size = file_dict[observation]
            tmp = dbc.Card([
                    dbc.CardHeader(f"{observation}"),
                    dbc.CardImg(src=mhi_file, top=True),
                    dbc.CardBody([
                        html.P(f"SUE = {sue:.2f}"),
                        html.P(f"Size = {file_size:.2f} Mb"),
                        # html.P(f"DQE = {dqe:.2f}")
                    ], style={'fontSize': 14,
                              'textAlign': 'center'}
                ),
            ],color="primary", outline=True)

        except:
            tmp = html.P()

        cl.append(tmp)

    onboard_sues, onboard_dds = get_sues_dds(onboard_dir)
    downlinked_sues, downlinked_dds = get_sues_dds(downlinked_dir)

    # RB Added Logic to account for no onboard data, or no data having been downlinked
    downlink_size_itc = len(downlinked_dds)
    uplink_size_itc = len(onboard_dds)

    if(downlink_size_itc > 0 and uplink_size_itc > 0):
        x_min = min(list(onboard_dds[:,0]) + list(downlinked_dds[:,0]))
        x_max = max(list(onboard_dds[:,0]) + list(downlinked_dds[:,0]))
        y_min = min(list(onboard_dds[:,1]) + list(downlinked_dds[:,1]))
        y_max = max(list(onboard_dds[:,1]) + list(downlinked_dds[:,1]))
    elif(downlink_size_itc <= 0 and uplink_size_itc > 0):
        x_min = min(list(onboard_dds[:,0]))
        x_max = max(list(onboard_dds[:,0]))
        y_min = min(list(onboard_dds[:,1]))
        y_max = max(list(onboard_dds[:,1]))
    elif(downlink_size_itc > 0 and uplink_size_itc <= 0):
        x_min = min(list(downlinked_dds[:,0]))
        x_max = max(list(downlinked_dds[:,0]))
        y_min = min(list(downlinked_dds[:,1]))
        y_max = max(list(downlinked_dds[:,1]))
    else:
        x_min = 0
        x_max = 0
        y_min = 0
        y_max = 0

    
    sue_min = min(list(onboard_sues) + list(downlinked_sues))
    sue_max = max(list(onboard_sues) + list(downlinked_sues))

    x_margin = abs(x_max - x_min) * 0.25
    y_margin = abs(y_max - y_min) * 0.25
    x_min -= x_margin
    x_max += x_margin
    y_min -= y_margin
    y_max += y_margin
    if x_min < 0:
        x_min = 0

    if y_min < 0:
        y_min = 0

# RB Logic added to update scatterplot when onboard or downlink are empty
    if(uplink_size_itc > 0):
        onboard_fig = px.scatter(x=onboard_dds[:,0], y=onboard_dds[:,1], color=onboard_sues, color_continuous_scale='viridis', range_color=(sue_min,sue_max))
        onboard_fig.update_layout(title={'text': "Prioritization on OWLS Data",'y':0.95,'x':0.5,'xanchor': 'center','yanchor': 'top'})
        onboard_fig.update_layout(xaxis_title=dd_readable_names[x_coord_dd_ind], yaxis_title=dd_readable_names[y_coord_dd_ind])
        onboard_fig.update_layout(coloraxis_colorbar=dict(title="SUE (Sci. Utility Est.)"))
        onboard_fig.update_traces(marker=dict(size=12, line=dict(width=1, color='black')), selector=dict(mode='markers'))
        onboard_fig.update_layout(coloraxis_colorbar_title_side="right")
        onboard_fig.update_layout(xaxis_range=[x_min,x_max], yaxis_range=[y_min,y_max])
    else:
        onboard_fig = {}

    if(downlink_size_itc > 0):
        downlinked_fig = px.scatter(x=downlinked_dds[:,0], y=downlinked_dds[:,1], color=downlinked_sues, color_continuous_scale='viridis', range_color=(sue_min,sue_max))
        downlinked_fig.update_layout(title={'text': "Prioritization on OWLS Data",'y':0.95,'x':0.5,'xanchor': 'center','yanchor': 'top'})
        downlinked_fig.update_layout(xaxis_title=dd_readable_names[x_coord_dd_ind], yaxis_title=dd_readable_names[y_coord_dd_ind])
        downlinked_fig.update_layout(coloraxis_colorbar=dict(title="SUE (Sci. Utility Est.)"))
        downlinked_fig.update_traces(marker=dict(size=12, line=dict(width=1, color='black')), selector=dict(mode='markers'))
        downlinked_fig.update_layout(coloraxis_colorbar_title_side="right")
        downlinked_fig.update_layout(xaxis_range=[x_min,x_max], yaxis_range=[y_min,y_max])
    else:
        downlinked_fig = {}

    return onboard_fig, downlinked_fig, cl[0], cl[1], cl[2], cl[3], cl[4], cl[5], cl[6], cl[7], cl[8], cl[9], cl[10], cl[11], cl[12], cl[13], cl[14], cl[15]


if __name__ == '__main__': 

    clear_folder(onboard_dir)
    if not os.path.exists(onboard_dir):
        os.makedirs(onboard_dir)

    clear_folder(downlinked_dir)
    if not os.path.exists(downlinked_dir):
        os.makedirs(downlinked_dir)

    app.run_server(debug=True, port=8051)





