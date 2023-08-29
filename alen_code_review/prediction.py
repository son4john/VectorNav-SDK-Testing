import numpy as np
from nltk.tokenize import RegexpTokenizer
from keras.models import Sequential, load_model
from keras.layers import LSTM
from keras.layers.core import Dense, Activation
from keras.optimizers import RMSprop
import pickle
import heapq
import PySimpleGUI as sg
import getpass 
import keyboard 
import time 


def nlp_predictive(): 
    """
    Returns the next five words of a text-query sent over. 
    Be warned that this service isn't precise and is in 
    beta testing for some more testing on its accuracy. 
    Remember only six-words is the maximum allowed for 
    the prediction service to guess words. 
    """
    username = getpass.getuser() 
    path = 'C:/Users/' + username + '/Autotyping Interface/Automatron Typing Interface/NLP_Processor/dataset1.txt'
    text = open(path, encoding="utf8").read().lower()

    tokenizer = RegexpTokenizer(r'\w+')
    words = tokenizer.tokenize(text)

    unique_words = np.unique(words)
    unique_word_index = dict((c, i) for i, c in enumerate(unique_words))

    WORD_LENGTH = 6
    prev_words = []
    next_words = []
    for i in range(len(words) - WORD_LENGTH):
        prev_words.append(words[i:i + WORD_LENGTH])
        next_words.append(words[i + WORD_LENGTH])

    X = np.zeros((len(prev_words), WORD_LENGTH, len(unique_words)), dtype=bool)
    Y = np.zeros((len(next_words), len(unique_words)), dtype=bool)
    for i, each_words in enumerate(prev_words):
        for j, each_word in enumerate(each_words):
            X[i, j, unique_word_index[each_word]] = 1
        Y[i, unique_word_index[next_words[i]]] = 1

    model = load_model('C:/Users/' + username + '/Autotyping Interface/Automatron Typing Interface/NLP_Processor/data_nlp.h5')
    history = pickle.load(open("C:/Users/" + username + "/Autotyping Interface/Automatron Typing Interface/NLP_Processor/history.p", "rb"))

    def prepare_input(text):
        x = np.zeros((1, WORD_LENGTH, len(unique_words)))
        for t, word in enumerate(text.split()):
            print(word)
            x[0, t, unique_word_index[word]] = 1
        return x

    prepare_input(" ".lower())

    def sample(preds, top_n=3):
        preds = np.asarray(preds).astype('float64')
        preds = np.log(preds)
        exp_preds = np.exp(preds)
        preds = exp_preds / np.sum(exp_preds)

        return heapq.nlargest(top_n, range(len(preds)), preds.take)

    def predict_completions(text, n=3):
        if text == "":
            return("0")
        x = prepare_input(text)
        preds = model.predict(x, verbose=0)[0]
        next_indices = sample(preds, n)
        return [unique_words[idx] for idx in next_indices]

    sg.theme("Dark Black")
    w, h = sg.Window.get_screen_size()
    layout = [[sg.Text("Predictive Text Service Window", font=("Segoe UI", 10))], 
             [sg.Text("Your Text input must NOT contain more than six words! Failure to put less than six words, will cause the program to error out!", font=("Segoe UI", 7))], 
             [sg.Input(size=(50, 50), k='-NLP_KY-')], 
             [sg.T(' ' * 14), sg.Submit('Submit Entry', k="-SUBMIT_PREDICT-", font=("Segoe UI", 9))],
             [sg.Text("(No words)", font=("Segoe UI", 8), justification='center', key='-PREDICTIVEOUTPUT-', size=(50, None))],
             [sg.Text("Keyword #1 - NONE", font=("Segoe UI", 8), key='-WRITELN1-', size=(15, None)), sg.T(' ' * 5), sg.Submit('Use Slice 1', k="-SLICE1-", font=("Segoe UI", 9))],
             [sg.Text("Keyword #2 - NONE", font=("Segoe UI", 8), key='-WRITELN2-', size=(15, None)), sg.T(' ' * 5), sg.Submit('Use Slice 2', k="-SLICE2-", font=("Segoe UI", 9))],
             [sg.Text("Keyword #3 - NONE", font=("Segoe UI", 8), key='-WRITELN3-', size=(15, None)), sg.T(' ' * 5), sg.Submit('Use Slice 3', k="-SLICE3-", font=("Segoe UI", 9))],
             [sg.Text("Keyword #4 - NONE", font=("Segoe UI", 8), key='-WRITELN4-', size=(15, None)), sg.T(' ' * 5), sg.Submit('Use Slice 4', k="-SLICE4-", font=("Segoe UI", 9))], 
             [sg.Text("Keyword #5 - NONE", font=("Segoe UI", 8), key='-WRITELN5-', size=(15, None)), sg.T(' ' * 5), sg.Submit('Use Slice 5', k="-SLICE5-", font=("Segoe UI", 9))]]

    icn_fle = 'C:/Users/' + username + '/Autotyping Interface/Automatron Typing Interface/logo_icn.ico'

    window = sg.Window('Predict Text', layout, size=(250, 320), location=(w - 260, h - 1080), return_keyboard_events=True, use_default_focus=False, icon=icn_fle, finalize=True)
    while True:
        event, values = window.read()
        if event == sg.WIN_CLOSED: # if user closes window or clicks cancel
            break 
        if event == "-SUBMIT_PREDICT-": 
            text = values['-NLP_KY-']
            try: 
                seq = " ".join(tokenizer.tokenize(text.lower())[0:5]) 
                dataLst = predict_completions(seq, 5)
                window.Element('-PREDICTIVEOUTPUT-').Update(value=dataLst)
                window.Element('-WRITELN1-').Update(value="Keyword #1 " + dataLst[0])
                window.Element('-WRITELN2-').Update(value="Keyword #2 " + dataLst[1])
                window.Element('-WRITELN3-').Update(value="Keyword #3 " + dataLst[2])
                window.Element('-WRITELN4-').Update(value="Keyword #4 " + dataLst[3])
                window.Element('-WRITELN5-').Update(value="Keyword #5 " + dataLst[4])
            except KeyError as word:
                err_msg_type1 = "Keyword not supported: " + str(word)
                window.Element('-PREDICTIVEOUTPUT-').Update(value=err_msg_type1)
                window.Element('-WRITELN1-').Update(value="Keyword #1 " + "Err Null")
                window.Element('-WRITELN2-').Update(value="Keyword #2 " + "Err Null")
                window.Element('-WRITELN3-').Update(value="Keyword #3 " + "Err Null")
                window.Element('-WRITELN4-').Update(value="Keyword #4 " + "Err Null")
                window.Element('-WRITELN5-').Update(value="Keyword #5 " + "Err Null")
            except IndexError: 
                sg.Popup("Please enter a string!", title="No String!")
                window.Element('-PREDICTIVEOUTPUT-').Update(value="(No words)")
                window.Element('-WRITELN1-').Update(value="Keyword #1 " + "Err Null")
                window.Element('-WRITELN2-').Update(value="Keyword #2 " + "Err Null")
                window.Element('-WRITELN3-').Update(value="Keyword #3 " + "Err Null")
                window.Element('-WRITELN4-').Update(value="Keyword #4 " + "Err Null")
                window.Element('-WRITELN5-').Update(value="Keyword #5 " + "Err Null")
        if event == "-SLICE1-":
            sg.Popup("Press F1 + Insert to assign your completed query. Please minimize the tab!", title="Keyword #1 Registered")
            predictive_word = dataLst[0]
            window.Hide()
            while True: 
                if keyboard.is_pressed("ins"):
                    keyboard.write(predictive_word) 
                    time.sleep(0.05)
                elif keyboard.is_pressed("del"):
                    window.UnHide()
                    break
        if event == "-SLICE2-":
            sg.Popup("Press F2 + Insert to assign your completed query", title="Keyword #2 Registered")
            predictive_word = dataLst[1]
            window.Hide()
            while True: 
                if keyboard.is_pressed("ins"):
                    keyboard.write(predictive_word) 
                    time.sleep(0.05)
                elif keyboard.is_pressed("del"):
                    window.UnHide()
                    break
        if event == "-SLICE3-":
            sg.Popup("Press F3 + Insert to assign your completed query", title="Keyword #3 Registered")
            predictive_word = dataLst[2]
            window.Hide()
            while True: 
                if keyboard.is_pressed("ins"):
                    keyboard.write(predictive_word) 
                    time.sleep(0.05)
                elif keyboard.is_pressed("del"):
                    window.UnHide()
                    break
        if event == "-SLICE4-":
            sg.Popup("Press F4 + Insert to assign your completed query", title="Keyword #4 Registered")
            predictive_word = dataLst[3]
            window.Hide()
            while True: 
                if keyboard.is_pressed("ins"):
                    keyboard.write(predictive_word) 
                    time.sleep(0.05)
                elif keyboard.is_pressed("del"):
                    window.UnHide()
                    break
        if event == "-SLICE5-":
            sg.Popup("Press F5 + Insert to assign your completed query", title="Keyword #5 Registered")
            predictive_word = dataLst[4]
            window.Hide()
            while True: 
                if keyboard.is_pressed("ins"):
                    keyboard.write(predictive_word) 
                    time.sleep(0.05)
                elif keyboard.is_pressed("del"):
                    window.UnHide()
                    break
    window.close() 