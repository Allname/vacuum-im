#include "emoticons.h"

#include <QSet>

#define SVN_SUBSTORAGES                 "substorages"

Emoticons::Emoticons()
{
  FMessageWidgets = NULL;
  FMessageProcessor = NULL;
  FSettingsPlugin = NULL;
}

Emoticons::~Emoticons()
{

}

void Emoticons::pluginInfo(IPluginInfo *APluginInfo)
{
  APluginInfo->author = "Potapov S.A. aka Lion";
  APluginInfo->description = tr("Insert smiley icons in chat windows");
  APluginInfo->homePage = "http://jrudevels.org";
  APluginInfo->name = tr("Emoticons"); 
  APluginInfo->uid = EMOTICONS_UUID;
  APluginInfo->version = "0.1";
}

bool Emoticons::initConnections(IPluginManager *APluginManager, int &/*AInitOrder*/)
{
  IPlugin *plugin = APluginManager->getPlugins("IMessageProcessor").value(0,NULL);
  if (plugin)
  {
    FMessageProcessor = qobject_cast<IMessageProcessor *>(plugin->instance());
  }

  plugin = APluginManager->getPlugins("IMessageWidgets").value(0,NULL);
  if (plugin)
  {
    FMessageWidgets = qobject_cast<IMessageWidgets *>(plugin->instance());
    if (FMessageWidgets)
    {
      connect(FMessageWidgets->instance(),SIGNAL(toolBarWidgetCreated(IToolBarWidget *)),SLOT(onToolBarWidgetCreated(IToolBarWidget *)));
    }
  }

  plugin = APluginManager->getPlugins("ISettingsPlugin").value(0,NULL);
  if (plugin) 
  {
    FSettingsPlugin = qobject_cast<ISettingsPlugin *>(plugin->instance());
    if (FSettingsPlugin)
    {
      connect(FSettingsPlugin->instance(),SIGNAL(settingsOpened()),SLOT(onSettingsOpened()));
      connect(FSettingsPlugin->instance(),SIGNAL(settingsClosed()),SLOT(onSettingsClosed()));
    }
  }

  return true;
}

bool Emoticons::initObjects()
{
  if (FMessageProcessor)
  {
    FMessageProcessor->insertMessageWriter(this,MWO_EMOTICONS);
  }

  if (FSettingsPlugin != NULL)
  {
    FSettingsPlugin->openOptionsNode(ON_EMOTICONS ,tr("Emoticons"),tr("Select emoticons files"),MNI_EMOTICONS,ONO_EMOTICONS);
    FSettingsPlugin->insertOptionsHolder(this);
  }

  return true;
}

void Emoticons::writeMessage(Message &/*AMessage*/, QTextDocument *ADocument, const QString &/*ALang*/, int AOrder)
{
  static QChar imageChar = QChar::ObjectReplacementCharacter;
  if (AOrder == MWO_EMOTICONS)
  {
    for (QTextCursor cursor = ADocument->find(imageChar); !cursor.isNull();  cursor = ADocument->find(imageChar,cursor))
    {
      QUrl url = cursor.charFormat().toImageFormat().name();
      QString key = keyByUrl(url);
      if (!key.isEmpty())
      {
        cursor.insertText(key);
        cursor.insertText(" ");
      }
    }
  }
}

void Emoticons::writeText(Message &/*AMessage*/, QTextDocument *ADocument, const QString &/*ALang*/, int AOrder)
{
  if (AOrder == MWO_EMOTICONS)
  {
    QRegExp regexp("\\S+");
    for (QTextCursor cursor = ADocument->find(regexp); !cursor.isNull();  cursor = ADocument->find(regexp,cursor))
    {
      QUrl url = FUrlByKey.value(cursor.selectedText());
      if (!url.isEmpty())
        cursor.insertImage(url.toString());
    }
  }
}

QWidget *Emoticons::optionsWidget(const QString &ANode, int &AOrder)
{
  if (ANode == ON_EMOTICONS)
  {
    AOrder = OWO_EMOTICONS;
    EmoticonsOptions *widget = new EmoticonsOptions(this);
    connect(widget,SIGNAL(optionsAccepted()),SIGNAL(optionsAccepted()));
    connect(FSettingsPlugin->instance(),SIGNAL(optionsDialogAccepted()),widget,SLOT(apply()));
    connect(FSettingsPlugin->instance(),SIGNAL(optionsDialogRejected()),SIGNAL(optionsRejected()));
    return widget;
  }
  return NULL;
}

void Emoticons::setIconsets(const QStringList &ASubStorages)
{
  QSet<QString> removeList = FStoragesOrder.toSet() - ASubStorages.toSet();
  foreach (QString substorage, removeList)
  {
    removeSelectIconMenu(substorage);
    FStoragesOrder.removeAt(FStoragesOrder.indexOf(substorage));
    delete FStorages.take(substorage);
    emit iconsetRemoved(substorage);
  }

  foreach (QString substorage, ASubStorages)
  {
    if (!FStoragesOrder.contains(substorage))
    {
      FStoragesOrder.append(substorage);
      FStorages.insert(substorage,new IconStorage(RSR_STORAGE_EMOTICONS,substorage,this));
      insertSelectIconMenu(substorage);
      emit iconsetInserted(substorage,"");
    }
  }
  FStoragesOrder = ASubStorages;
  createIconsetUrls();
}

void Emoticons::insertIconset(const QString &ASubStorage, const QString &ABefour)
{
  if (!FStoragesOrder.contains(ASubStorage))
  {
    ABefour.isEmpty() ? FStoragesOrder.append(ASubStorage) : FStoragesOrder.insert(FStoragesOrder.indexOf(ABefour),ASubStorage);
    FStorages.insert(ASubStorage,new IconStorage(RSR_STORAGE_EMOTICONS,ASubStorage,this));
    insertSelectIconMenu(ASubStorage);
    createIconsetUrls();
    emit iconsetInserted(ASubStorage,ABefour);
  }
}

void Emoticons::removeIconset(const QString &ASubStorage)
{
  if (FStoragesOrder.contains(ASubStorage))
  {
    removeSelectIconMenu(ASubStorage);
    FStoragesOrder.removeAt(FStoragesOrder.indexOf(ASubStorage));
    delete FStorages.take(ASubStorage);
    createIconsetUrls();
    emit iconsetRemoved(ASubStorage);
  }
}

QUrl Emoticons::urlByKey(const QString &AKey) const
{
  return FUrlByKey.value(AKey);
}

QString Emoticons::keyByUrl(const QUrl &AUrl) const
{
  return FUrlByKey.key(AUrl);
}

void Emoticons::createIconsetUrls()
{
  FUrlByKey.clear();
  foreach(QString substorage, FStoragesOrder)
  {
    IconStorage *storage = FStorages.value(substorage);
    foreach(QString key, storage->fileKeys())
    {
      if (!FUrlByKey.contains(key))
        FUrlByKey.insert(key,QUrl::fromLocalFile(storage->fileFullName(key)));
    }
  }
}

SelectIconMenu *Emoticons::createSelectIconMenu(const QString &ASubStorage, QWidget *AParent)
{
  SelectIconMenu *menu = new SelectIconMenu(AParent);
  menu->setIconset(ASubStorage);
  connect(menu->instance(),SIGNAL(iconSelected(const QString &, const QString &)),
    SLOT(onIconSelected(const QString &, const QString &)));
  connect(menu->instance(),SIGNAL(destroyed(QObject *)),SLOT(onSelectIconMenuDestroyed(QObject *)));
  return menu;
}

void Emoticons::insertSelectIconMenu(const QString &AIconsetFile)
{
  foreach(IToolBarWidget *widget, FToolBarsWidgets)
  {
    SelectIconMenu *menu = createSelectIconMenu(AIconsetFile,widget->instance());
    FToolBarWidgetByMenu.insert(menu,widget);
    QToolButton *button = widget->toolBarChanger()->addToolButton(menu->menuAction(),TBG_MWCW_EMOTICONS,false);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setPopupMode(QToolButton::InstantPopup);
  }
}

void Emoticons::removeSelectIconMenu(const QString &AIconsetFile)
{
  QMap<SelectIconMenu *,IToolBarWidget *>::iterator it = FToolBarWidgetByMenu.begin();
  while (it != FToolBarWidgetByMenu.end())
  {
    SelectIconMenu *menu = it.key();
    IToolBarWidget *widget = it.value();
    if (menu->iconset() == AIconsetFile)
    {
      widget->toolBarChanger()->removeAction(menu->menuAction());
      it = FToolBarWidgetByMenu.erase(it);
      delete menu;
    }
    else
      it++;
  }
}

void Emoticons::onToolBarWidgetCreated(IToolBarWidget *AWidget)
{
  if (AWidget->editWidget() != NULL)
  {
    FToolBarsWidgets.append(AWidget);
    foreach(QString iconsetFile, FStoragesOrder)
    {
      SelectIconMenu *menu = createSelectIconMenu(iconsetFile,AWidget->instance());
      FToolBarWidgetByMenu.insert(menu,AWidget);
      QToolButton *button = AWidget->toolBarChanger()->addToolButton(menu->menuAction(),TBG_MWCW_EMOTICONS);
      button->setToolButtonStyle(Qt::ToolButtonIconOnly);
      button->setPopupMode(QToolButton::InstantPopup);
    }
    connect(AWidget->instance(),SIGNAL(destroyed(QObject *)),SLOT(onToolBarWidgetDestroyed(QObject *)));
  }
}

void Emoticons::onToolBarWidgetDestroyed(QObject *AObject)
{
  FToolBarsWidgets.removeAt(FToolBarsWidgets.indexOf((IToolBarWidget *)AObject));
}

void Emoticons::onIconSelected(const QString &/*ASubStorage*/, const QString &AIconKey)
{
  SelectIconMenu *menu = qobject_cast<SelectIconMenu *>(sender());
  if (FToolBarWidgetByMenu.contains(menu))
  {
    IEditWidget *widget = FToolBarWidgetByMenu.value(menu)->editWidget();
    if (widget)
    {
      widget->textEdit()->setFocus();
      widget->textEdit()->textCursor().insertText(AIconKey);
      widget->textEdit()->textCursor().insertText(" ");
    }
  }
}

void Emoticons::onSelectIconMenuDestroyed(QObject *AObject)
{
  FToolBarWidgetByMenu.remove((SelectIconMenu *)AObject);
}

void Emoticons::onSettingsOpened()
{
  ISettings *settings = FSettingsPlugin->settingsForPlugin(EMOTICONS_UUID);
  setIconsets(settings->value(SVN_SUBSTORAGES,FileStorage::availSubStorages(RSR_STORAGE_EMOTICONS)).toStringList());
}

void Emoticons::onSettingsClosed()
{
  ISettings *settings = FSettingsPlugin->settingsForPlugin(EMOTICONS_UUID);
  settings->setValue(SVN_SUBSTORAGES,FStoragesOrder);
}

Q_EXPORT_PLUGIN2(EmoticonsPlugin, Emoticons)
