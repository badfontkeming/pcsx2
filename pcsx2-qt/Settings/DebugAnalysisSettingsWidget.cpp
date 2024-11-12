// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "DebugAnalysisSettingsWidget.h"

#include "SettingsWindow.h"
#include "SettingWidgetBinder.h"

#include "DebugTools/SymbolImporter.h"

#include <QtWidgets/QFileDialog>

DebugAnalysisSettingsWidget::DebugAnalysisSettingsWidget(QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);

	m_ui.automaticallyClearSymbols->setChecked(Host::GetBoolSettingValue("Debugger/Analysis", "AutomaticallySelectSymbolsToClear", true));

	setupSymbolSourceGrid();

	m_ui.importFromElf->setChecked(Host::GetBoolSettingValue("Debugger/Analysis", "ImportSymbolsFromELF", true));
	m_ui.importSymFileFromDefaultLocation->setChecked(Host::GetBoolSettingValue("Debugger/Analysis", "ImportSymFileFromDefaultLocation", true));
	m_ui.demangleSymbols->setChecked(Host::GetBoolSettingValue("Debugger/Analysis", "DemangleSymbols", true));
	m_ui.demangleParameters->setChecked(Host::GetBoolSettingValue("Debugger/Analysis", "DemangleParameters", true));

	setupSymbolFileList();

	std::string function_scan_mode = Host::GetStringSettingValue("Debugger/Analysis", "FunctionScanMode");
	for (int i = 0;; i++)
	{
		if (Pcsx2Config::DebugAnalysisOptions::FunctionScanModeNames[i] == nullptr)
			break;

		if (function_scan_mode == Pcsx2Config::DebugAnalysisOptions::FunctionScanModeNames[i])
			m_ui.functionScanMode->setCurrentIndex(i);
	}

	m_ui.customAddressRange->setChecked(Host::GetBoolSettingValue("Debugger/Analysis", "CustomFunctionScanRange", false));
	m_ui.addressRangeStart->setText(QString::fromStdString(Host::GetStringSettingValue("Debugger/Analysis", "FunctionScanStartAddress", "0")));
	m_ui.addressRangeEnd->setText(QString::fromStdString(Host::GetStringSettingValue("Debugger/Analysis", "FunctionScanEndAddress", "0")));

	m_ui.grayOutOverwrittenFunctions->setChecked(Host::GetBoolSettingValue("Debugger/Analysis", "GenerateFunctionHashes", true));

	connect(m_ui.automaticallyClearSymbols, &QCheckBox::checkStateChanged, this, &DebugAnalysisSettingsWidget::updateEnabledStates);
	connect(m_ui.demangleSymbols, &QCheckBox::checkStateChanged, this, &DebugAnalysisSettingsWidget::updateEnabledStates);
	connect(m_ui.customAddressRange, &QCheckBox::checkStateChanged, this, &DebugAnalysisSettingsWidget::updateEnabledStates);

	updateEnabledStates();
}

DebugAnalysisSettingsWidget::DebugAnalysisSettingsWidget(SettingsWindow* dialog, QWidget* parent)
	: QWidget(parent)
	, m_dialog(dialog)
{
	SettingsInterface* sif = dialog->getSettingsInterface();

	m_ui.setupUi(this);

	// Make sure the user doesn't select symbol sources from both the global
	// settings and the per-game settings, as these settings will conflict with
	// each other. It only really makes sense to modify these settings on a
	// per-game basis anyway.
	if (dialog->isPerGameSettings())
	{
		SettingWidgetBinder::BindWidgetToBoolSetting(
			sif, m_ui.automaticallyClearSymbols, "Debugger/Analysis", "AutomaticallySelectSymbolsToClear", true);

		m_dialog->registerWidgetHelp(m_ui.automaticallyClearSymbols, tr("Automatically Select Symbols To Clear"), tr("Checked"),
			tr("Automatically delete symbols that were generated by any previous analysis runs."));

		setupSymbolSourceGrid();
	}
	else
	{
		m_ui.clearExistingSymbolsGroup->hide();
	}

	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, m_ui.importFromElf, "Debugger/Analysis", "ImportSymbolsFromELF", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, m_ui.importSymFileFromDefaultLocation, "Debugger/Analysis", "ImportSymFileFromDefaultLocation", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, m_ui.demangleSymbols, "Debugger/Analysis", "DemangleSymbols", true);
	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, m_ui.demangleParameters, "Debugger/Analysis", "DemangleParameters", true);

	m_dialog->registerWidgetHelp(m_ui.importFromElf, tr("Import From ELF"), tr("Checked"),
		tr("Import symbol tables stored in the game's boot ELF."));
	m_dialog->registerWidgetHelp(m_ui.importSymFileFromDefaultLocation, tr("Import Default .sym File"), tr("Checked"),
		tr("Import symbols from a .sym file with the same name as the loaded ISO file on disk if such a file exists."));
	m_dialog->registerWidgetHelp(m_ui.demangleSymbols, tr("Demangle Symbols"), tr("Checked"),
		tr("Demangle C++ symbols during the import process so that the function and global variable names shown in the "
		   "debugger are more readable."));
	m_dialog->registerWidgetHelp(m_ui.demangleParameters, tr("Demangle Parameters"), tr("Checked"),
		tr("Include parameter lists in demangled function names."));

	// Same as above. It only makes sense to load extra symbol files on a
	// per-game basis.
	if (dialog->isPerGameSettings())
	{
		setupSymbolFileList();
	}
	else
	{
		m_ui.symbolFileLabel->hide();
		m_ui.symbolFileList->hide();
		m_ui.importSymbolFileButtons->hide();
	}

	SettingWidgetBinder::BindWidgetToEnumSetting(
		sif, m_ui.functionScanMode, "Debugger/Analysis", "FunctionScanMode",
		Pcsx2Config::DebugAnalysisOptions::FunctionScanModeNames, DebugFunctionScanMode::SCAN_ELF);

	m_dialog->registerWidgetHelp(m_ui.functionScanMode, tr("Scan Mode"), tr("Scan ELF"),
		tr("Choose where the function scanner looks to find functions. This option can be useful if the application "
		   "loads additional code at runtime."));

	// Same as above. It only makes sense to set a custom memory range on a
	// per-game basis.
	if (dialog->isPerGameSettings())
	{
		SettingWidgetBinder::BindWidgetToBoolSetting(
			sif, m_ui.customAddressRange, "Debugger/Analysis", "CustomFunctionScanRange", false);
		connect(m_ui.addressRangeStart, &QLineEdit::textChanged, this, &DebugAnalysisSettingsWidget::functionScanRangeChanged);
		connect(m_ui.addressRangeEnd, &QLineEdit::textChanged, this, &DebugAnalysisSettingsWidget::functionScanRangeChanged);

		m_dialog->registerWidgetHelp(m_ui.customAddressRange, tr("Custom Address Range"), tr("Unchecked"),
			tr("Whether to look for functions from the address range specified (Checked), or from the ELF segment "
			   "containing the entry point (Unchecked)."));
	}
	else
	{
		m_ui.customAddressRange->hide();
		m_ui.customAddressRangeLineEdits->hide();
	}

	SettingWidgetBinder::BindWidgetToBoolSetting(
		sif, m_ui.grayOutOverwrittenFunctions, "Debugger/Analysis", "GenerateFunctionHashes", true);

	m_dialog->registerWidgetHelp(m_ui.grayOutOverwrittenFunctions, tr("Gray Out Symbols For Overwritten Functions"), tr("Checked"),
		tr("Generate hashes for all the detected functions, and gray out the symbols displayed in the debugger for "
		   "functions that no longer match."));

	connect(m_ui.automaticallyClearSymbols, &QCheckBox::checkStateChanged, this, &DebugAnalysisSettingsWidget::updateEnabledStates);
	connect(m_ui.demangleSymbols, &QCheckBox::checkStateChanged, this, &DebugAnalysisSettingsWidget::updateEnabledStates);
	connect(m_ui.customAddressRange, &QCheckBox::checkStateChanged, this, &DebugAnalysisSettingsWidget::updateEnabledStates);

	updateEnabledStates();
}

void DebugAnalysisSettingsWidget::parseSettingsFromWidgets(Pcsx2Config::DebugAnalysisOptions& output)
{
	output.AutomaticallySelectSymbolsToClear = m_ui.automaticallyClearSymbols->isChecked();

	for (const auto& [name, temp] : m_symbol_sources)
	{
		DebugSymbolSource& source = output.SymbolSources.emplace_back();
		source.Name = name;
		source.ClearDuringAnalysis = temp.check_box->isChecked();
	}

	output.ImportSymbolsFromELF = m_ui.importFromElf->isChecked();
	output.ImportSymFileFromDefaultLocation = m_ui.importSymFileFromDefaultLocation->isChecked();
	output.DemangleSymbols = m_ui.demangleSymbols->isChecked();
	output.DemangleParameters = m_ui.demangleParameters->isChecked();

	for (int i = 0; i < m_ui.symbolFileList->count(); i++)
	{
		DebugExtraSymbolFile& file = output.ExtraSymbolFiles.emplace_back();
		file.Path = m_ui.symbolFileList->item(i)->text().toStdString();
	}

	output.FunctionScanMode = static_cast<DebugFunctionScanMode>(m_ui.functionScanMode->currentIndex());
	output.CustomFunctionScanRange = m_ui.customAddressRange->isChecked();
	output.FunctionScanStartAddress = m_ui.addressRangeStart->text().toStdString();
	output.FunctionScanEndAddress = m_ui.addressRangeEnd->text().toStdString();

	output.GenerateFunctionHashes = m_ui.grayOutOverwrittenFunctions->isChecked();
}

void DebugAnalysisSettingsWidget::setupSymbolSourceGrid()
{
	QGridLayout* layout = new QGridLayout(m_ui.symbolSourceGrid);

	if (!m_dialog || m_dialog->getSerial() == QtHost::GetCurrentGameSerial().toStdString())
	{
		// Add symbol sources for which the user has already selected whether or
		// not they should be cleared.
		int existing_symbol_source_count;
		if (m_dialog)
			existing_symbol_source_count = m_dialog->getEffectiveIntValue("Debugger/Analysis/SymbolSources", "Count", 0);
		else
			existing_symbol_source_count = Host::GetIntSettingValue("Debugger/Analysis/SymbolSources", "Count", 0);

		for (int i = 0; i < existing_symbol_source_count; i++)
		{
			std::string section = "Debugger/Analysis/SymbolSources/" + std::to_string(i);

			std::string name;
			if (m_dialog)
				name = m_dialog->getEffectiveStringValue(section.c_str(), "Name", "");
			else
				name = Host::GetStringSettingValue(section.c_str(), "Name", "");

			bool value;
			if (m_dialog)
				value = m_dialog->getEffectiveBoolValue(section.c_str(), "ClearDuringAnalysis", false);
			else
				value = Host::GetBoolSettingValue(section.c_str(), "ClearDuringAnalysis", false);

			SymbolSourceTemp& source = m_symbol_sources[name];
			source.previous_value = value;
			source.modified_by_user = true;
		}

		// Add any more symbol sources for which the user hasn't made a
		// selection. These are separate since we don't want to have to store
		// configuration data for them.
		R5900SymbolGuardian.Read([&](const ccc::SymbolDatabase& database) {
			for (const ccc::SymbolSource& symbol_source : database.symbol_sources)
			{
				if (m_symbol_sources.find(symbol_source.name()) == m_symbol_sources.end() && symbol_source.name() != "Built-In")
				{
					SymbolSourceTemp& source = m_symbol_sources[symbol_source.name()];
					source.previous_value = SymbolImporter::ShouldClearSymbolsFromSourceByDefault(symbol_source.name());
					source.modified_by_user = false;
				}
			}
		});

		if (m_symbol_sources.empty())
		{
			m_ui.symbolSourceErrorMessage->setText(tr("<i>No symbol sources in database.</i>"));
			m_ui.symbolSourceScrollArea->hide();
			return;
		}

		// Create the check boxes.
		int i = 0;
		for (auto& [name, temp] : m_symbol_sources)
		{
			temp.check_box = new QCheckBox(QString::fromStdString(name));
			temp.check_box->setChecked(temp.previous_value);
			layout->addWidget(temp.check_box, i / 2, i % 2);

			connect(temp.check_box, &QCheckBox::checkStateChanged, this, &DebugAnalysisSettingsWidget::symbolSourceCheckStateChanged);

			i++;
		}
	}
	else
	{
		m_ui.symbolSourceErrorMessage->setText(tr("<i>Start this game to modify the symbol sources list.</i>"));
		m_ui.symbolSourceScrollArea->hide();
		return;
	}

	m_ui.symbolSourceErrorMessage->hide();
}

void DebugAnalysisSettingsWidget::symbolSourceCheckStateChanged()
{
	QCheckBox* check_box = qobject_cast<QCheckBox*>(sender());
	if (!check_box)
		return;

	auto temp = m_symbol_sources.find(check_box->text().toStdString());
	if (temp == m_symbol_sources.end())
		return;

	temp->second.modified_by_user = true;

	saveSymbolSources();
}

void DebugAnalysisSettingsWidget::saveSymbolSources()
{
	if (!m_dialog)
		return;

	SettingsInterface* sif = m_dialog->getSettingsInterface();
	if (!sif)
		return;

	// Clean up old configuration entries.
	int old_count = sif->GetIntValue("Debugger/Analysis/SymbolSources", "Count");
	for (int i = 0; i < old_count; i++)
	{
		std::string section = "Debugger/Analysis/SymbolSources/" + std::to_string(i);
		sif->RemoveSection(section.c_str());
	}

	sif->RemoveSection("Debugger/Analysis/SymbolSources");

	int symbol_sources_to_save = 0;
	for (auto& [name, temp] : m_symbol_sources)
		if (temp.modified_by_user)
			symbol_sources_to_save++;

	if (symbol_sources_to_save == 0)
		return;

	// Make new configuration entries.
	sif->SetIntValue("Debugger/Analysis/SymbolSources", "Count", symbol_sources_to_save);

	int i = 0;
	for (auto& [name, temp] : m_symbol_sources)
	{
		if (!temp.modified_by_user)
			continue;

		std::string section = "Debugger/Analysis/SymbolSources/" + std::to_string(i);
		sif->SetStringValue(section.c_str(), "Name", name.c_str());
		sif->SetBoolValue(section.c_str(), "ClearDuringAnalysis", temp.check_box->isChecked());

		i++;
	}
}

void DebugAnalysisSettingsWidget::setupSymbolFileList()
{
	int extra_symbol_file_count;
	if (m_dialog)
		extra_symbol_file_count = m_dialog->getEffectiveIntValue("Debugger/Analysis/ExtraSymbolFiles", "Count", 0);
	else
		extra_symbol_file_count = Host::GetIntSettingValue("Debugger/Analysis/ExtraSymbolFiles", "Count", 0);

	for (int i = 0; i < extra_symbol_file_count; i++)
	{
		std::string section = "Debugger/Analysis/ExtraSymbolFiles/" + std::to_string(i);
		std::string path;
		if (m_dialog)
			path = m_dialog->getEffectiveStringValue(section.c_str(), "Path", "");
		else
			path = Host::GetStringSettingValue(section.c_str(), "Path", "");

		m_ui.symbolFileList->addItem(QString::fromStdString(path));
	}

	connect(m_ui.addSymbolFile, &QPushButton::clicked, this, &DebugAnalysisSettingsWidget::addSymbolFile);
	connect(m_ui.removeSymbolFile, &QPushButton::clicked, this, &DebugAnalysisSettingsWidget::removeSymbolFile);
}

void DebugAnalysisSettingsWidget::addSymbolFile()
{
	QString path = QDir::toNativeSeparators(QFileDialog::getOpenFileName(this, tr("Add Symbol File")));
	if (path.isEmpty())
		return;

	m_ui.symbolFileList->addItem(path);

	saveSymbolFiles();
}

void DebugAnalysisSettingsWidget::removeSymbolFile()
{
	for (QListWidgetItem* item : m_ui.symbolFileList->selectedItems())
		delete item;

	saveSymbolFiles();
}

void DebugAnalysisSettingsWidget::saveSymbolFiles()
{
	if (!m_dialog)
		return;

	SettingsInterface* sif = m_dialog->getSettingsInterface();
	if (!sif)
		return;

	// Clean up old configuration entries.
	int old_count = sif->GetIntValue("Debugger/Analysis/ExtraSymbolFiles", "Count");
	for (int i = 0; i < old_count; i++)
	{
		std::string section = "Debugger/Analysis/ExtraSymbolFiles/" + std::to_string(i);
		sif->RemoveSection(section.c_str());
	}

	sif->RemoveSection("Debugger/Analysis/ExtraSymbolFiles");

	if (m_ui.symbolFileList->count() == 0)
		return;

	// Make new configuration entries.
	sif->SetIntValue("Debugger/Analysis/ExtraSymbolFiles", "Count", m_ui.symbolFileList->count());

	for (int i = 0; i < m_ui.symbolFileList->count(); i++)
	{
		std::string section = "Debugger/Analysis/ExtraSymbolFiles/" + std::to_string(i);
		std::string path = m_ui.symbolFileList->item(i)->text().toStdString();
		sif->SetStringValue(section.c_str(), "Path", path.c_str());
	}

	QtHost::SaveGameSettings(sif, true);
	g_emu_thread->reloadGameSettings();
}

void DebugAnalysisSettingsWidget::functionScanRangeChanged()
{
	if (!m_dialog)
		return;

	SettingsInterface* sif = m_dialog->getSettingsInterface();
	if (!sif)
		return;

	QString start_address = m_ui.addressRangeStart->text();
	QString end_address = m_ui.addressRangeEnd->text();

	bool ok;

	if (start_address.toUInt(&ok, 16), ok)
		sif->SetStringValue("Debugger/Analysis", "FunctionScanStartAddress", start_address.toStdString().c_str());

	if (end_address.toUInt(&ok, 16), ok)
		sif->SetStringValue("Debugger/Analysis", "FunctionScanEndAddress", end_address.toStdString().c_str());
}

void DebugAnalysisSettingsWidget::updateEnabledStates()
{
	m_ui.symbolSourceScrollArea->setEnabled(!m_ui.automaticallyClearSymbols->isChecked());
	m_ui.symbolSourceErrorMessage->setEnabled(!m_ui.automaticallyClearSymbols->isChecked());
	m_ui.demangleParameters->setEnabled(m_ui.demangleSymbols->isChecked());
	m_ui.customAddressRangeLineEdits->setEnabled(m_ui.customAddressRange->isChecked());
}