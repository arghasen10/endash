import smtplib
from email.mime.text import MIMEText

def sendErrorMail(sub, msg, pss):

    frm = "abhimondal@iitkgpmail.iitkgp.ac.in"
    to = "abhijitmanpur@gmail.com,abhimondal@iitkgp.ac.in"
    body = msg


    s = smtplib.SMTP("iitkgpmail.iitkgp.ac.in")
    s.login("abhimondal", pss)
    me="abhimondal@iitkgp.ac.in"

    msg = MIMEText(msg)
    msg['From'] = me
    msg['To'] = to
    msg['Subject'] = sub
    s.sendmail(me, to.split(","), msg.as_string())
    s.quit()
