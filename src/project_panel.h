#pragma once
#include <wx/panel.h>
#include <wx/treectrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/choice.h>
#include <wx/button.h>
#include <functional>
#include <set>
#include <string>

// ---------------------------------------------------------------------------
// Data attached to every node in the project tree.
// ---------------------------------------------------------------------------
struct TreeNode : public wxTreeItemData {
    enum class Kind { Folder, Project, Article };
    Kind        kind;
    std::string path;              // absolute filesystem path (dir or .md file)
    std::string name;              // display name
    std::string parentProjectPath; // for Article: the owning project dir
    std::string parentProjectName; // for Article: the owning project name
};

class ProjectPanel : public wxPanel {
public:
    using OpenCallback = std::function<void(const std::string&)>;

    ProjectPanel(wxWindow* parent, OpenCallback onProjectActivated, bool darkMode = false);

    void SetDarkMode(bool dark);
    void RefreshProjects();

private:
    // ---- tree helpers -------------------------------------------------------
    // Populate children of parentId from dirPath up to depth levels.
    // query is the current search string (empty = show all).
    // Returns true when at least one project node was added (used for
    // filtering: a folder that contributes nothing is not added).
    bool PopulateTree(wxTreeItemId parentId,
                      const std::string& dirPath,
                      int depth,
                      const std::string& query,
                      int sortOrder);   // 0=Name 1=Created 2=Modified

    // ---- selection / activation ---------------------------------------------
    TreeNode* SelectedNode() const;
    void ActivateSelectedProject();

    // ---- event handlers -----------------------------------------------------
    void OnSearchChanged(wxCommandEvent& evt);
    void OnSortChanged(wxCommandEvent& evt);
    void OnActivateBtn(wxCommandEvent& evt);
    void OnRenameBtn(wxCommandEvent& evt);
    void OnNewBtn(wxCommandEvent& evt);
    void OnDeleteBtn(wxCommandEvent& evt);
    void OnRefreshBtn(wxCommandEvent& evt);
    void OnSetFolderBtn(wxCommandEvent& evt);
    void OnKebabBtn(wxCommandEvent& evt);

    // Shared creation used by the New… dialog and (later) by "Initialize as
    // project" on an existing folder.
    void CreateFolderAt(const std::string& parentPath, const std::string& name);
    void CreateProjectAt(const std::string& parentPath, const std::string& name);

    void OnTreeSelChanged(wxTreeEvent& evt);
    void OnTreeItemActivated(wxTreeEvent& evt);
    void OnTreeExpanding(wxTreeEvent& evt);
    void OnTreeCollapsing(wxTreeEvent& evt);
    void OnTreeBeginDrag(wxTreeEvent& evt);
    void OnTreeEndDrag(wxTreeEvent& evt);
    void OnTreeItemMenu(wxTreeEvent& evt);
    void OnCtxInitProject(wxCommandEvent& evt);
    void OnCtxReveal(wxCommandEvent& evt);

    // ---- widgets ------------------------------------------------------------
    wxTextCtrl*   m_searchCtrl;
    wxChoice*     m_sortChoice;
    wxTreeCtrl*   m_treeCtrl;
    wxStaticText* m_projectPathLabel;
    wxStaticText* m_statsLabel;
    wxButton*     m_activateBtn;
    wxButton*     m_renameBtn;
    wxButton*     m_newBtn;
    wxButton*     m_deleteBtn;
    wxButton*     m_kebabBtn;

    void ApplyTheme();

    // ---- state --------------------------------------------------------------
    bool                  m_darkMode = false;
    std::set<std::string> m_expandedPaths;   // paths whose folder nodes are expanded
    wxTreeItemId          m_dragItem;        // item being dragged
    OpenCallback          m_openCallback;

    wxDECLARE_EVENT_TABLE();
};
