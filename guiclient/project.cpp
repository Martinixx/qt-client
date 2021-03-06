  /*
 * This file is part of the xTuple ERP: PostBooks Edition, a free and
 * open source Enterprise Resource Planning software suite,
 * Copyright (c) 1999-2012 by OpenMFG LLC, d/b/a xTuple.
 * It is licensed to you under the Common Public Attribution License
 * version 1.0, the full text of which (including xTuple-specific Exhibits)
 * is available at www.xtuple.com/CPAL.  By using this software, you agree
 * to be bound by its terms.
 */

#include "project.h"

#include <QVariant>
#include <QMessageBox>
#include <QSqlError>
#include <openreports.h>
#include <comment.h>
#include <metasql.h>
#include "mqlutil.h"
#include "errorReporter.h"
#include "guiErrorCheck.h"
#include "task.h"
#include "dspOrderActivityByProject.h"

const char *_projectStatuses[] = { "P", "O", "C" };

project::project(QWidget* parent, const char* name, bool modal, Qt::WFlags fl)
    : XDialog(parent, name, modal, fl)
{
  setupUi(this);

  if(!_privileges->check("EditOwner")) _owner->setEnabled(false);

  connect(_buttonBox,     SIGNAL(rejected()),        this, SLOT(sClose()));
  connect(_buttonBox,     SIGNAL(accepted()),        this, SLOT(sSave()));
  connect(_printTasks,    SIGNAL(clicked()),         this, SLOT(sPrintTasks()));
  connect(_newTask,       SIGNAL(clicked()),         this, SLOT(sNewTask()));
  connect(_editTask,      SIGNAL(clicked()),         this, SLOT(sEditTask()));
  connect(_viewTask,      SIGNAL(clicked()),         this, SLOT(sViewTask()));
  connect(_deleteTask,    SIGNAL(clicked()),         this, SLOT(sDeleteTask()));
  connect(_number,        SIGNAL(editingFinished()), this, SLOT(sNumberChanged()));
  connect(_activity,      SIGNAL(clicked()),         this, SLOT(sActivity()));
  connect(_crmacct,       SIGNAL(newId(int)),        this, SLOT(sCRMAcctChanged(int)));

  _prjtask->addColumn( tr("Number"),            _itemColumn,    Qt::AlignLeft,  true, "prjtask_number" );
  _prjtask->addColumn( tr("Name"),              _itemColumn,    Qt::AlignLeft,  true, "prjtask_name"  );
  _prjtask->addColumn( tr("Description"),                -1,    Qt::AlignLeft,  true, "prjtask_descrip" );
  _prjtask->addColumn( tr("Hours Balance"),     _itemColumn,    Qt::AlignRight, true, "prjtaskhrbal" );
  _prjtask->addColumn( tr("Expense Balance"),   _itemColumn,    Qt::AlignRight, true, "prjtaskexpbal" );

  _owner->setUsername(omfgThis->username());
  _assignedTo->setUsername(omfgThis->username());
  _owner->setType(UsernameLineEdit::UsersActive);
  _assignedTo->setType(UsernameLineEdit::UsersActive);

  _totalHrBud->setPrecision(omfgThis->qtyVal());
  _totalHrAct->setPrecision(omfgThis->qtyVal());
  _totalHrBal->setPrecision(omfgThis->qtyVal());
  _totalExpBud->setPrecision(omfgThis->moneyVal());
  _totalExpAct->setPrecision(omfgThis->moneyVal());
  _totalExpBal->setPrecision(omfgThis->moneyVal());
  
  _saved=false;

  populate();
}

project::~project()
{
  // no need to delete child widgets, Qt does it all for us
}

void project::languageChange()
{
  retranslateUi(this);
}

enum SetResponse project::set(const ParameterList &pParams)
{
  XSqlQuery projectet;
  XDialog::set(pParams);
  QVariant param;
  bool     valid;

  param = pParams.value("username", &valid);
  if (valid)
    _assignedTo->setUsername(param.toString());

  param = pParams.value("prj_id", &valid);
  if (valid)
  {
    _prjid = param.toInt();
    populate();
  }

  param = pParams.value("crmacct_id", &valid);
  if (valid)
  {
    _crmacct->setId(param.toInt());
    _crmacct->setEnabled(false);
  }

  param = pParams.value("cntct_id", &valid);
  if (valid)
  {
    _cntct->setId(param.toInt());
  }

  param = pParams.value("mode", &valid);
  if (valid)
  {
    if (param.toString() == "new")
    {
      _mode = cNew;

      connect(_assignedTo, SIGNAL(newId(int)), this, SLOT(sAssignedToChanged(int)));
      connect(_status,  SIGNAL(currentIndexChanged(int)), this, SLOT(sStatusChanged(int)));
      connect(_prjtask, SIGNAL(valid(bool)), _editTask, SLOT(setEnabled(bool)));
      connect(_prjtask, SIGNAL(valid(bool)), _deleteTask, SLOT(setEnabled(bool)));
      connect(_prjtask, SIGNAL(itemSelected(int)), _editTask, SLOT(animateClick()));

      projectet.exec("SELECT NEXTVAL('prj_prj_id_seq') AS prj_id;");
      if (projectet.first())
        _prjid = projectet.value("prj_id").toInt();
      else if (projectet.lastError().type() == QSqlError::NoError)
      {
        systemError(this, projectet.lastError().text(), __FILE__, __LINE__);
        return UndefinedError;
      }

      _comments->setId(_prjid);
      _documents->setId(_prjid);
      _recurring->setParent(_prjid, "J");
    }
    else if (param.toString() == "edit")
    {
      _mode = cEdit;

      _number->setEnabled(FALSE);

      connect(_assignedTo, SIGNAL(newId(int)), this, SLOT(sAssignedToChanged(int)));
      connect(_status,  SIGNAL(currentIndexChanged(int)), this, SLOT(sStatusChanged(int)));
      connect(_prjtask, SIGNAL(valid(bool)), _editTask, SLOT(setEnabled(bool)));
      connect(_prjtask, SIGNAL(valid(bool)), _deleteTask, SLOT(setEnabled(bool)));
      connect(_prjtask, SIGNAL(itemSelected(int)), _editTask, SLOT(animateClick()));
    }
    else if (param.toString() == "view")
    {
      _mode = cView;
      
      _owner->setEnabled(FALSE);
      _number->setEnabled(FALSE);
      _status->setEnabled(FALSE);
      _name->setEnabled(FALSE);
      _descrip->setEnabled(FALSE);
      _so->setEnabled(FALSE);
      _wo->setEnabled(FALSE);
      _po->setEnabled(FALSE);
      _assignedTo->setEnabled(FALSE);
      _crmacct->setEnabled(false);
      _cntct->setEnabled(false);
      _newTask->setEnabled(FALSE);
      connect(_prjtask, SIGNAL(itemSelected(int)), _viewTask, SLOT(animateClick()));
      _comments->setReadOnly(TRUE);
      _documents->setReadOnly(TRUE);
      _started->setEnabled(FALSE);
      _assigned->setEnabled(FALSE);
      _due->setEnabled(FALSE);
      _completed->setEnabled(FALSE);
      _recurring->setEnabled(FALSE);
      _buttonBox->removeButton(_buttonBox->button(QDialogButtonBox::Save));
      _buttonBox->removeButton(_buttonBox->button(QDialogButtonBox::Cancel));
      _buttonBox->addButton(QDialogButtonBox::Close);
    }
  }
    
  return NoError;
}

void project::populate()
{
  XSqlQuery projectpopulate;
  projectpopulate.prepare( "SELECT * "
             "FROM prj "
             "WHERE (prj_id=:prj_id);" );
  projectpopulate.bindValue(":prj_id", _prjid);
  projectpopulate.exec();
  if (projectpopulate.first())
  {
    _saved = true;
    _owner->setUsername(projectpopulate.value("prj_owner_username").toString());
    _number->setText(projectpopulate.value("prj_number").toString());
    _name->setText(projectpopulate.value("prj_name").toString());
    _descrip->setText(projectpopulate.value("prj_descrip").toString());
    _so->setChecked(projectpopulate.value("prj_so").toBool());
    _wo->setChecked(projectpopulate.value("prj_wo").toBool());
    _po->setChecked(projectpopulate.value("prj_po").toBool());
    _assignedTo->setUsername(projectpopulate.value("prj_username").toString());
    _cntct->setId(projectpopulate.value("prj_cntct_id").toInt());
    _crmacct->setId(projectpopulate.value("prj_crmacct_id").toInt());
    _started->setDate(projectpopulate.value("prj_start_date").toDate());
    _assigned->setDate(projectpopulate.value("prj_assigned_date").toDate());
    _due->setDate(projectpopulate.value("prj_due_date").toDate());
    _completed->setDate(projectpopulate.value("prj_completed_date").toDate());
    for (int counter = 0; counter < _status->count(); counter++)
    {
      if (QString(projectpopulate.value("prj_status").toString()[0]) == _projectStatuses[counter])
        _status->setCurrentIndex(counter);
    }

    _recurring->setParent(projectpopulate.value("prj_recurring_prj_id").isNull() ?
                            _prjid : projectpopulate.value("prj_recurring_prj_id").toInt(),
                          "J");
  }

  sFillTaskList();
  _comments->setId(_prjid);
  _documents->setId(_prjid);
  emit populated(_prjid);
}

void project::sAssignedToChanged(const int newid)
{
  if (newid == -1)
    _assigned->clear();
  else
    _assigned->setDate(omfgThis->dbDate());
}

void project::sStatusChanged(const int pStatus)
{
  switch(pStatus)
  {
    case 0: // Concept
    default:
      _started->clear();
      _completed->clear();
      break;
    case 1: // In Process
      _started->setDate(omfgThis->dbDate());
      _completed->clear();
      break;
    case 2: // Completed
      _completed->setDate(omfgThis->dbDate());
      break;
  }
}

void project::sCRMAcctChanged(const int newid)
{
  _cntct->setSearchAcct(newid);
}

void project::sClose()
{
  XSqlQuery projectClose;
  if (_mode == cNew)
  {
    projectClose.prepare( "SELECT deleteproject(:prj_id);" );
    projectClose.bindValue(":prj_id", _prjid);
    projectClose.exec();
  }

  reject();
}

bool project::sSave(bool partial)
{
  XSqlQuery projectSave;
  QList<GuiErrorCheck> errors;
  errors<< GuiErrorCheck(_number->text().isEmpty(), _number,
                         tr("No Project Number was specified. You must specify a project number "
                            "before saving it."))
        << GuiErrorCheck(!partial && !_due->isValid(), _due,
                         tr("You must specify a due date before "
                            "saving it."))
  ;
  if (GuiErrorCheck::reportErrors(this, tr("Cannot Save Project"), errors))
    return false;

  RecurrenceWidget::RecurrenceChangePolicy cp = _recurring->getChangePolicy();
  if (cp == RecurrenceWidget::NoPolicy)
    return false;

  XSqlQuery rollbackq;
  rollbackq.prepare("ROLLBACK;");
  XSqlQuery begin("BEGIN;");

  if (!_saved)
    projectSave.prepare( "INSERT INTO prj "
               "( prj_id, prj_number, prj_name, prj_descrip,"
               "  prj_so, prj_wo, prj_po, prj_status, prj_owner_username, "
               "  prj_start_date, prj_due_date, prj_assigned_date,"
               "  prj_completed_date, prj_username, prj_recurring_prj_id,"
               "  prj_crmacct_id, prj_cntct_id) "
               "VALUES "
               "( :prj_id, :prj_number, :prj_name, :prj_descrip,"
               "  :prj_so, :prj_wo, :prj_po, :prj_status, :prj_owner_username,"
               "  :prj_start_date, :prj_due_date, :prj_assigned_date,"
               "  :prj_completed_date, :username, :prj_recurring_prj_id,"
               "  :prj_crmacct_id, :prj_cntct_id);" );
  else
    projectSave.prepare( "UPDATE prj "
               "SET prj_number=:prj_number, prj_name=:prj_name, prj_descrip=:prj_descrip,"
               "    prj_so=:prj_so, prj_wo=:prj_wo, prj_po=:prj_po, prj_status=:prj_status, "
               "    prj_owner_username=:prj_owner_username, prj_start_date=:prj_start_date, "
               "    prj_due_date=:prj_due_date, prj_assigned_date=:prj_assigned_date,"
               "    prj_completed_date=:prj_completed_date,"
               "    prj_username=:username,"
               "    prj_recurring_prj_id=:prj_recurring_prj_id,"
               "    prj_crmacct_id=:prj_crmacct_id,"
               "    prj_cntct_id=:prj_cntct_id "
               "WHERE (prj_id=:prj_id);" );

  projectSave.bindValue(":prj_id", _prjid);
  projectSave.bindValue(":prj_number", _number->text().trimmed().toUpper());
  projectSave.bindValue(":prj_name", _name->text());
  projectSave.bindValue(":prj_descrip", _descrip->toPlainText());
  projectSave.bindValue(":prj_status", _projectStatuses[_status->currentIndex()]);
  projectSave.bindValue(":prj_so", QVariant(_so->isChecked()));
  projectSave.bindValue(":prj_wo", QVariant(_wo->isChecked()));
  projectSave.bindValue(":prj_po", QVariant(_po->isChecked()));
  projectSave.bindValue(":prj_owner_username", _owner->username());
  projectSave.bindValue(":username", _assignedTo->username());
  if (_crmacct->id() > 0)
    projectSave.bindValue(":prj_crmacct_id", _crmacct->id());
  if (_cntct->id() > 0)
    projectSave.bindValue(":prj_cntct_id", _cntct->id());
  projectSave.bindValue(":prj_start_date", _started->date());
  projectSave.bindValue(":prj_due_date",	_due->date());
  projectSave.bindValue(":prj_assigned_date", _assigned->date());
  projectSave.bindValue(":prj_completed_date", _completed->date());
  if (_recurring->isRecurring())
    projectSave.bindValue(":prj_recurring_prj_id", _recurring->parentId());

  projectSave.exec();
  if (projectSave.lastError().type() != QSqlError::NoError)
  {
    rollbackq.exec();
    systemError(this, projectSave.lastError().databaseText(), __FILE__, __LINE__);
    return false;
  }

  QString errmsg;
  if (! _recurring->save(true, cp, &errmsg))
  {
    qDebug("recurring->save failed");
    rollbackq.exec();
    systemError(this, errmsg, __FILE__, __LINE__);
    return false;
  }

  projectSave.exec("COMMIT;");
  emit saved(_prjid);

  if (!partial)
    done(_prjid);
  else
    _saved=true;
  return true;
}

void project::sPrintTasks()
{
  ParameterList params;

  params.append("prj_id", _prjid);

  orReport report("ProjectTaskList", params);
  if(report.isValid())
    report.print();
  else
    report.reportError(this);
}

void project::sNewTask()
{
  if (!_saved)
  {
    if (!sSave(true))
      return;
  }
    
  ParameterList params;
  params.append("mode", "new");
  params.append("prj_id", _prjid);
  params.append("prj_owner_username", _owner->username());
  params.append("prj_username", _assignedTo->username());
  params.append("prj_start_date", _started->date());
  params.append("prj_due_date",	_due->date());
  params.append("prj_assigned_date", _assigned->date());
  params.append("prj_completed_date", _completed->date());

  task newdlg(this, "", TRUE);
  newdlg.set(params);
  if (newdlg.exec() != XDialog::Rejected)
    sFillTaskList();
}

void project::sEditTask()
{
  ParameterList params;
  params.append("mode", "edit");
  params.append("prjtask_id", _prjtask->id());

  task newdlg(this, "", TRUE);
  newdlg.set(params);
  if (newdlg.exec() != XDialog::Rejected)
    sFillTaskList();
}

void project::sViewTask()
{
  ParameterList params;
  params.append("mode", "view");
  params.append("prjtask_id", _prjtask->id());

  task newdlg(this, "", TRUE);
  newdlg.set(params);
  newdlg.exec();
}

void project::sDeleteTask()
{
  XSqlQuery projectDeleteTask;
  projectDeleteTask.prepare("SELECT deleteProjectTask(:prjtask_id) AS result; ");
  projectDeleteTask.bindValue(":prjtask_id", _prjtask->id());
  projectDeleteTask.exec();
  if(projectDeleteTask.first())
  {
    int result = projectDeleteTask.value("result").toInt();
    if(result < 0)
    {
      QString errmsg;
      switch(result)
      {
        case -1:
          errmsg = tr("Project task not found.");
          break;
        case -2:
          errmsg = tr("Actual hours have been posted to this project task.");
          break;
        case -3:
          errmsg = tr("Actual expenses have been posted to this project task.");
          break;
        default:
          errmsg = tr("Error #%1 encountered while trying to delete project task.").arg(result);
      }
      QMessageBox::critical( this, tr("Cannot Delete Project Task"),
        tr("Could not delete the project task for one or more reasons.\n") + errmsg);
      return;
    }
  }
  emit deletedTask();
  sFillTaskList();
}

void project::sFillTaskList()
{
  MetaSQLQuery mql = mqlLoad("projectTasks", "detail");

  ParameterList params;
  params.append("prj_id", _prjid);
  XSqlQuery qry = mql.toQuery(params);
  if (qry.first())
  {
    _totalHrBud->setDouble(qry.value("totalhrbud").toDouble());
    _totalHrAct->setDouble(qry.value("totalhract").toDouble());
    _totalHrBal->setDouble(qry.value("totalhrbal").toDouble());
    _totalExpBud->setDouble(qry.value("totalexpbud").toDouble());
    _totalExpAct->setDouble(qry.value("totalexpact").toDouble());
    _totalExpBal->setDouble(qry.value("totalexpbal").toDouble());
  }
  if (qry.lastError().type() != QSqlError::NoError)
  {
    systemError(this, qry.lastError().databaseText(), __FILE__, __LINE__);
    return;
  }
  else
  {
    _totalHrBud->setDouble(0.0);
    _totalHrAct->setDouble(0.0);
    _totalHrBal->setDouble(0.0);
    _totalExpBud->setDouble(0.0);
    _totalExpAct->setDouble(0.0);
    _totalExpBal->setDouble(0.0);
  }

  _prjtask->populate(qry);
}

void project::sNumberChanged()
{
  XSqlQuery projectNumberChanged;
  if((cNew == _mode) && (_number->text().length()))
  {
    _number->setText(_number->text().trimmed().toUpper());

    projectNumberChanged.prepare( "SELECT prj_id"
               "  FROM prj"
               " WHERE (prj_number=:prj_number);" );
    projectNumberChanged.bindValue(":prj_number", _number->text());
    projectNumberChanged.exec();
    if(projectNumberChanged.first())
    {
      _number->setEnabled(FALSE);
      _prjid = projectNumberChanged.value("prj_id").toInt();
      _mode = cEdit;
      populate();
    }
    else
    {
      _number->setEnabled(FALSE);
      _mode = cNew;
    }
  }
}

void project::sActivity()
{
  ParameterList params;
  params.append("prj_id", _prjid);
  params.append("run", true);

  dspOrderActivityByProject *newdlg = new dspOrderActivityByProject(this,"dspOrderActivityByProject",Qt::Dialog );
  newdlg->set(params);
  omfgThis->handleNewWindow(newdlg);
}
